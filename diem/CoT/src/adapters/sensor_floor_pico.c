/* *****************************************************************************
 * Copyright (c) 2009, 2010 Andrey Litvinov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * ****************************************************************************/
/** @file
 * This is an adapter for the Elsi Sensor Floor.
 * It uses Pico server HTTP interface to get the data from the floor.
 * Usually, Pico server has data feed at the URL
 * http://<server.name>/<room>/feed
 * The adapter reads data about people positions on the floor and publish it
 * to the static objects at the oBIX server.
 *
 * @author Andrey Litvinov
 * @version 1.1
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <log_utils.h>
#include <ixml_ext.h>
#include <xml_config.h>
#include <ptask.h>
#include <obix_utils.h>
#include <obix_client.h>

/** Id of the connection to the target oBIX server. */
#define SERVER_CONNECTION 0

/** Timeout after which the target is considered unavailable. */
#define TARGET_REMOVE_TIMEOUT 15000

/** Target object which describes person's position on the sensor floor. */
typedef struct _Target
{
    /** Constant address of the target object at the oBIX server. */
    char* uri;
    /** X coordinate of the person. */
    char* x;
    /** Y coordinate of the person. */
    char* y;
    /** Id of this target at the sensor floor */
    int id;
    /** Defines whether this target new or not. */
    BOOL new;
    /** Defines whether this target has been changed recently. */
    BOOL changed;
    /** Id of the task which is scheduled to remove this target when it is not
     * changed for #TARGET_REMOVE_TIMEOUT milliseconds.*/
    int removeTask;
}
Target;

/** Contains device data which is published to the oBIX server. */
IXML_Document* _deviceData;
/** Head of the targets list. */
Target* _targets;
/** Number of targets in the targets list. */
int _targetsCount;
/** Id of the device which is registered at the oBIX server. */
int _deviceIdShift;
/** Thread for scheduling asynchronous tasks. */
Task_Thread* _taskThread;
/** Mutex for thread synchronization: Targets are changed from one thread and
 * removed from another. */
pthread_mutex_t _targetMutex = PTHREAD_MUTEX_INITIALIZER;

/** Contains data for testing. @see testCycle(). */
IXML_Document* _testData;

// declaration of the ..
void targetRemoveTask(void* arg);

/** @name Targets management
 * Utility methods for work with target object.
 * @{
 */
/**
 * Sets X value of the target object.
 *
 * @param target Target object which should be changed.
 * @param newValue New X value.
 */
void target_setX(Target* target, const char* newValue)
{
    if (target->x != NULL)
    {
        free(target->x);
    }
    target->x = (char*) malloc(strlen(newValue) + 1);
    strcpy(target->x, newValue);

    target->changed = TRUE;
}

/**
 * Sets Y value of the target object.
 *
 * @param target Target object which should be changed.
 * @param newValue New Y value.
 */
void target_setY(Target* target, const char* newValue)
{
    if (target->y != NULL)
    {
        free(target->y);
    }
    target->y = (char*) malloc(strlen(newValue) + 1);
    strcpy(target->y, newValue);

    target->changed = TRUE;
}

/**
 * Searches for target with specified Id.
 *
 * @param id Id of the target which should be found.
 * @return Target with specified Id. If no such target found, search for a free
 *         target object and assigns provided Id to it. If there are no free
 *         targets, @a NULL is returned.
 */
Target* target_get(int id)
{
    int i;
    for (i = 0; i < _targetsCount; i++)
    {
        if (_targets[i].id == id)
        {
            return &(_targets[i]);
        }
    }

    // no target with provided id is found
    // return empty target
    for (i = 0; i < _targetsCount; i++)
    {
        if (_targets[i].id == 0)
        {
            Target* target = &_targets[i];
            target->id = id;
            target->new = TRUE;
            target->removeTask = ptask_schedule(_taskThread,
                                                &targetRemoveTask,
                                                target,
                                                TARGET_REMOVE_TIMEOUT,
                                                1);
            return target;
        }
    }

    // no free slots
    return NULL;
}

/**
 * Sends update of the target's values to the oBIX server.
 *
 * @param target Target object which should be sent.
 * @return #OBIX_SUCCESS if data is successfully sent, error code otherwise.
 */
int target_sendUpdate(Target* target)
{
    // send new X and Y values to the server
    // we send several values to the server simultaneously, that's why we use
    // Batch object.
    oBIX_Batch* batch = obix_batch_create(SERVER_CONNECTION);
    if (batch == NULL)
    {	// that can hardly ever happen... e.g. when there is no enough memory
        return OBIX_ERR_NO_MEMORY;
    }

    // generate URI for x coordinate
    int error = 0;
    int targetUriLength = strlen(target->uri);
    char fullUri[targetUriLength + 10];
    strcpy(fullUri, target->uri);


    // send update of available attribute if needed
    if (target->new == TRUE)
    {
        strcat(fullUri, "available");

        error = obix_batch_writeValue(batch,
                                      _deviceIdShift,
                                      fullUri,
                                      XML_TRUE,
                                      OBIX_T_BOOL);
        target->new = FALSE;
    }
    else if (target->changed == FALSE)
    {
        // target is being removed
        strcat(fullUri, "available");

        error = obix_batch_writeValue(batch,
                                      _deviceIdShift,
                                      fullUri,
                                      XML_FALSE,
                                      OBIX_T_BOOL);
    }

    if (error < 0)
    {	// unable to generate a batch command
        return error;
    }

    // send update of X coordinate
    fullUri[targetUriLength] = 'x';
    fullUri[targetUriLength + 1] = '\0';

    error = obix_batch_writeValue(batch,
                                  _deviceIdShift,
                                  fullUri,
                                  target->x,
                                  OBIX_T_REAL);
    if (error < 0)
    {
        return error;
    }

    // send Y update
    fullUri[targetUriLength] = 'y';
    fullUri[targetUriLength + 1] = '\0';

    error = obix_batch_writeValue(batch,
                                  _deviceIdShift,
                                  fullUri,
                                  target->y,
                                  OBIX_T_REAL);
    if (error < 0)
    {
        return error;
    }

    // now send the batch object which contains all required write operations
    error = obix_batch_send(batch);
    // and don't forget to free the batch object after use
    obix_batch_free(batch);
    if (error != OBIX_SUCCESS)
    {
        return error;
    }

    target->changed = FALSE;
    return OBIX_SUCCESS;
}
/** @} */

/**
 * Parses target object received from the sensor floor.
 *
 * @param node XML representation of the received target object.
 * @return @a 0 on success, @a -1 on error.
 */
int parseTarget(IXML_Node* node)
{
    if (!obix_obj_implementsContract(ixmlNode_convertToElement(node),
                                     "target"))
    {
    	// it is not a target - nothing to parse
    	return 0;
    }

    node = ixmlNode_getFirstChild(node);
    // first node is id tag (at least we hope so :)
    const char* attrValue = ixmlElement_getAttribute(
                                ixmlNode_convertToElement(node), OBIX_ATTR_VAL);
    int id = atoi(attrValue);
    if (id <= 0)
    {
        char* buffer = ixmlPrintNode(node);
        log_error("Unable to parse target: "
                  "ID expected in the first child tag, but it is:\n%s",
                  buffer);
        free(buffer);
        return -1;
    }
    // get corresponding target
    Target* target = target_get(id);
    if (target == NULL)
    {
        // there is no free slot to monitor this target - ignore it
        return 0;
    }

    // iterate through the whole list of target params
    while (node != NULL)
    {
        IXML_Element* param = ixmlNode_convertToElement(node);
        if (param == NULL)
        {
            log_error("Target contains something unexpected.");
            return -1;
        }

        const char* paramName = ixmlElement_getAttribute(
                                    param,
                                    OBIX_ATTR_NAME);
        if (paramName == NULL)
        {
            log_error("Target object doesn't have \"%s\" attribute.",
                      OBIX_ATTR_NAME);
            return -1;
        }

        if (strcmp(paramName, "x") == 0)
        {
            const char* newValue = ixmlElement_getAttribute(param,
                                   OBIX_ATTR_VAL);
            if (newValue == NULL)
            {
                return -1;
            }
            target_setX(target, newValue);
        }
        else if (strcmp(paramName, "y") == 0)
        {
            const char* newValue = ixmlElement_getAttribute(param,
                                   OBIX_ATTR_VAL);
            if (newValue == NULL)
            {
                return -1;
            }
            target_setY(target, newValue);
        }

        node = ixmlNode_getNextSibling(node);
    }

    return 0;
}

/**
 * Task which clears target object at the oBIX server if it is not changed for
 * #TARGET_REMOVE_TIMEOUT milliseconds.
 * It is scheduled for each target object. Implements #periodic_task prototype.
 *
 * @param arg Reference to the target object which should be removed.
 */
void targetRemoveTask(void* arg)
{
    Target* target = (Target*) arg;

    pthread_mutex_lock(&_targetMutex);

    if (target->changed == TRUE)
    {
        // target is updated
        // no need to remove it now
        ptask_reset(_taskThread, target->removeTask);
        pthread_mutex_unlock(&_targetMutex);
        return;
    }

    // clean target
    target->id = 0;
    target->new = FALSE;
    target->removeTask = 0;
    target_setX(target, "0");
    target_setY(target, "0");
    target->changed = FALSE;

    // send update to the server
    if (target_sendUpdate(target) != OBIX_SUCCESS)
    {
        log_error("Unable to update the target at oBIX server.");
    }

    pthread_mutex_unlock(&_targetMutex);
}

/**
 * Checks whether targets are updated. If yes - sends updated values to the
 * server.
 *
 * @return 0 on success, -1 on error
 */
int checkTargets()
{
    int i;
    for (i = 0; i < _targetsCount; i++)
    {
        Target* target = &(_targets[i]);
        if (target->id == 0)
        {
            // this is empty target slot - ignore it
            continue;
        }

        if (target->changed == TRUE)
        {
            // send update to the server
            if (target_sendUpdate(target) != OBIX_SUCCESS)
            {
                return -1;
            }

            // reset timer of the remove task
            ptask_reset(_taskThread, target->removeTask);
        }
    }

    return 0;
}

/**
 * Listens to target updates at the sensor floor.
 * Implements #obix_update_listener prototype.
 * @ref obix_update_listener
 */
int targetsListener(int connectionId,
                    int deviceId,
                    int listenerId,
                    const char* newValue)
{
    //    log_warning("Targets update received:\n%s\n", newValue);
    // parse update
    IXML_Document* doc;
    int error = ixmlParseBufferEx(newValue, &doc);
    if (error != IXML_SUCCESS)
    {
        log_error("Unable to parse received targets update:\n%s", newValue);
        return -1;
    }

    pthread_mutex_lock(&_targetMutex);
    IXML_NodeList* targets = ixmlDocument_getElementsByTagName(doc, OBIX_OBJ);
    if (targets == NULL)
    {
        // no targets - ignore.
        ixmlDocument_free(doc);
        error = checkTargets();
        pthread_mutex_unlock(&_targetMutex);
        return error;
    }

    int i;
    int targetsCount = ixmlNodeList_length(targets);
    if (targetsCount > _targetsCount)
    {
        log_warning("Sensor Floor returned %d targets, "
                    "but we monitor only %d.", targetsCount, _targetsCount);
        targetsCount = _targetsCount;
    }

    for (i = 0; i < targetsCount; i++)
    {
        if (parseTarget(ixmlNodeList_item(targets, i)) != 0)
        {
            // error occurred, stop parsing
            log_error("Unable to parse received targets update:\n%s",
                      newValue);
            break;
        }
    }

    ixmlNodeList_free(targets);
    ixmlDocument_free(doc);

    //check targets and send updates if necessary
    checkTargets();
    pthread_mutex_unlock(&_targetMutex);

    return 0;
}

/**
 * Sets the @a fall variable at the oBIX server to @a true if the fallen event
 * is received from the sensor floor.
 * @param events XML document which contains received events from the sensor
 *               floor.
 * @return @li @a 1 If fallen even is detected and the @a fall variable is
 *         changed on the oBIX server.
 *         @li @a 0 If fallen event is not detected.
 *         @li @a -1 If error occurred when update was sent to the server.
 */
int sendFallEventUpdate(IXML_Document* events)
{
    // this variable is introduced to filter several calls of the method
    // caused by one fallen event
    // TODO check how good it is
    static BOOL receivedFallEvent = FALSE;

    // we look not only for FALLEN event
    // PULSE probably means that the floor detected pulse of the fallen person
    // NOVITALS by this logic means that the floor can't detect any movements
    // of the fallen person
    if (ixmlDocument_getElementByAttrValue(events,
                                           OBIX_ATTR_VAL,
                                           "FALLEN") != NULL
            || ixmlDocument_getElementByAttrValue(events,
                                                  OBIX_ATTR_VAL,
                                                  "PULSE") != NULL
            || ixmlDocument_getElementByAttrValue(events,
                                                  OBIX_ATTR_VAL,
                                                  "NOVITALS") != NULL)
    {
        // fall event occurred! notify oBIX server if it is not done yet
        if (receivedFallEvent == FALSE)
        {
            receivedFallEvent = TRUE;
            // lock the mutex because we use connection to the server which can
            // be used from another thread simultaneously
            pthread_mutex_lock(&_targetMutex);
            log_warning("Somebody fell down..\n");
            int error = obix_writeValue(SERVER_CONNECTION,
                                        _deviceIdShift,
                                        "fall",
                                        XML_TRUE,
                                        OBIX_T_BOOL);
            pthread_mutex_unlock(&_targetMutex);

            if (error != OBIX_SUCCESS)
            {
                log_error("Unable to notify oBIX server about fall event.");
                return -1;
            }
        }
        return 1;
    }

    if (ixmlDocument_getElementById(events, OBIX_OBJ) != NULL)
    {	// we received some another event
        receivedFallEvent = FALSE;
    }

    return 0;
}

/**
 * Listens to the event feed of the sensor floor.
 * Implements #obix_update_listener.
 * @ref obix_update_listener
 */
int eventFeedListener(int connectionId,
                      int deviceId,
                      int listenerId,
                      const char* newValue)
{
    // parse update
    IXML_Document* doc;
    int error = ixmlParseBufferEx(newValue, &doc);
    if (error != IXML_SUCCESS)
    {
        log_error("Unable to parse received event feed update:\n%s", newValue);
        return -1;
    }

    //    if (ixmlDocument_getElementById(doc, OBIX_OBJ) != NULL)
    //    {
    //    if (strncmp(newValue, "<feed", 5) != 0)
    //    {
    //        log_debug("Received new event:\n%s", newValue);
    //    }
    //    }

    // check whether we have fallen event
    error = sendFallEventUpdate(doc);
    if (error == 1)
    {
        error = 0;
        log_debug("Fallen even is caught by feed listener");
    }

    ixmlDocument_free(doc);
    return error;
}

/**
 * Generates device data which will be published on the oBIX server.
 * Uses settings from configuration file.
 *
 * @param deviceConf XML configuration loaded from the file.
 * return @a 0 on success, @a -1 on error.
 */
int loadDeviceData(IXML_Element* deviceConf)
{
    if (deviceConf == NULL)
    {
        return -1;
    }

    // get device object stub
    IXML_Element* deviceData = config_getChildTag(deviceConf, OBIX_OBJ, TRUE);
    if (deviceData == NULL)
    {
        return -1;
    }

    // create a copy of device data
    deviceData = ixmlElement_cloneWithLog(deviceData, TRUE);
    IXML_Document* doc = ixmlNode_getOwnerDocument(
                             ixmlElement_getNode(deviceData));

    // get target settings
    IXML_Element* target = config_getChildTag(deviceConf, "target", TRUE);
    if (target == NULL)
    {
        ixmlDocument_free(doc);
        return -1;
    }

    int targetsCount = config_getTagAttrIntValue(target, "count", TRUE, 1);
    if (targetsCount <= 0)
    {
        ixmlDocument_free(doc);
        return -1;
    }

    _targets = (Target*) calloc(targetsCount, sizeof(Target));
    _targetsCount = targetsCount;

    // get target stub
    target = config_getChildTag(target, OBIX_OBJ, TRUE);
    if (target == NULL)
    {
        ixmlDocument_free(doc);
        return -1;
    }

    // generate required amount of targets
    int i;
    IXML_Node* node;
    for (i = 0; i < targetsCount; i++)
    {
        ixmlDocument_importNode(doc, ixmlElement_getNode(target), TRUE, &node);
        ixmlNode_appendChild(ixmlElement_getNode(deviceData), node);

        // generate name and href of the new target
        char name[9];
        char displayName[10];
        char* href = (char*) malloc(10);

        sprintf(name, "target%d", i + 1);
        sprintf(displayName, "Target %d", i + 1);
        sprintf(href, "target%d/", i + 1);

        IXML_Element* t = ixmlNode_convertToElement(node);
        ixmlElement_setAttribute(t, OBIX_ATTR_NAME, name);
        ixmlElement_setAttribute(t, OBIX_ATTR_DISPLAY_NAME, displayName);
        ixmlElement_setAttribute(t, OBIX_ATTR_HREF, href);
        // save target
        _targets[i].uri = href;
    }

    _deviceData = doc;
    return 0;
}

/**
 * Loads driver settings from the specified configuration file.
 *
 * @param fileName Name of the configuration file.
 * @return @a 0 on success, @a -1 on error.
 */
int loadSettings(const char* fileName)
{
    IXML_Element* settings = config_loadFile(fileName);
    if (settings == NULL)
    {
        return -1;
    }
    // configure log system
    int error = config_log(settings);
    if (error != 0)
    {
        return -1;
    }

    // initialize obix_client library with loaded settings
    error = obix_loadConfig(settings);
    if (error != OBIX_SUCCESS)
    {
        config_finishInit(settings, FALSE);
        return error;
    }

    // load data which will be posted to oBIX server from settings file
    IXML_Element* element = config_getChildTag(settings, "device-info", TRUE);
    if (loadDeviceData(element) != 0)
    {
        return -1;
    }

    // TODO for testing purposes only
//    // load test data
//    element = config_getChildTag(settings, "test-data", TRUE);
//    element = ixmlElement_cloneWithLog(element, TRUE);
//    _testData = ixmlNode_getOwnerDocument(ixmlElement_getNode(element));

    // finish initialization
    config_finishInit(settings, TRUE);
    return 0;
}

/**
 * Emulates target data from the sensor floor.
 * Test data is read from the configuration file and sent step by step
 * to the targets listener. For testing purposes only.
 */
void testCycle()
{
    IXML_NodeList* testList = ixmlDocument_getElementsByTagName(_testData,
                              OBIX_OBJ_LIST);
    int testCount = ixmlNodeList_length(testList);

    int i;
    for (i = 0; i < testCount; i++)
    {
        log_warning("Test step #%d from %d. Press Enter to continue.\n",
                    i, testCount);
        getchar();

        char* testData = ixmlPrintNode(ixmlNodeList_item(testList, i));
        targetsListener(SENSOR_FLOOR_CONNECTION, 0, 0, testData);
        free(testData);
    }

    ixmlNodeList_free(testList);
    ixmlDocument_free(_testData);
}

/**
 * Sets all targets at server to 'not available' and
 * removes all target values.
 */
void resetTargets()
{
    int i;
    for (i = 0; i < _targetsCount; i++)
    {
        if (_targets[i].id != 0)
        {
            targetRemoveTask(&(_targets[i]));
        }

        free(_targets[i].x);
        free(_targets[i].y);
        free(_targets[i].uri);
    }

    free(_targets);
}

/**
 * Checks periodically the last sensor floor event and invokes event feed
 * listener.
 * @todo it is created because Sensor Floor's event feed doesn't show the Fallen
 * event
 * @param args Parameter is not used (created to match #periodic_task prototype)
 */
void checkEventsTask(void* args)
{
    IXML_Element* lastEvent;
    int error = obix_read(SENSOR_FLOOR_FEED_CONNECTION,
                          0,
                          "/obix/elsi/Stok/event/",
                          &lastEvent);

    if (error != OBIX_SUCCESS)
    {
        log_error("Unable to read last floor event.");
        return;
    }

    error = sendFallEventUpdate(ixmlNode_getOwnerDocument(
                                    ixmlElement_getNode(lastEvent)));
    if (error == 1)
    {
        log_debug("Fallen event is caught by checking last event.");
    }

    ixmlElement_freeOwnerDocument(lastEvent);
}

/**
 * Entry point of the driver.
 *
 * @param argc It takes exactly 1 input argument.
 * @param argv The only argument is the name of configuration file.
 * @return @a 0 on success, negative error code otherwise.
 */
int main(int argc, char** argv)
{
    if (argc != 2)
    {
        printf("Usage: sensor_floor_pico <config_file>\n"
               " where <config_file> - Name of the configuration file.\n");
        return -1;
    }

    // load settings from file
    if (loadSettings(argv[1]) != 0)
    {
        printf("Unable to load settings from file %s.\n", argv[1]);
        return -1;
    }

    // initialize the task thread
    _taskThread = ptask_init();
    if (_taskThread == NULL)
    {
        log_error("Unable to initialize separate thread.");
    }

    // open connection to the Sensor Floor
    int error = obix_openConnection(SENSOR_FLOOR_CONNECTION);
    if (error != OBIX_SUCCESS)
    {
        log_error("Unable to establish connection with the Sensor Floor.\n");
        return error;
    }

    // open second connection to the Sensor Floor (for events)
    error = obix_openConnection(SENSOR_FLOOR_FEED_CONNECTION);
    if (error != OBIX_SUCCESS)
    {
        log_error("Unable to establish second connection with the Sensor "
                  "Floor.\n");
        return error;
    }

    // open connection to the target oBIX server
    error = obix_openConnection(SERVER_CONNECTION);
    if (error != OBIX_SUCCESS)
    {
        log_error("Unable to establish connection with oBIX server.\n");
        return error;
    }

    // register Sensor Floor at the target oBIX server
    char* deviceData = ixmlPrintDocument(_deviceData);
    _deviceIdShift = obix_registerDevice(SERVER_CONNECTION, deviceData);
    free(deviceData);
    if (_deviceIdShift < 0)
    {
        log_error("Unable to register Sensor Floor at oBIX server.\n");
        return -1;
    }

    // register listener for targets list of the Sensor Floor
    int targetListenerId = obix_registerListener(SENSOR_FLOOR_CONNECTION,
                           0,
                           "/obix/elsi/Stok/targets/",
                           &targetsListener);
    if (targetListenerId < 0)
    {
        log_error("Unable to register update listener for targets.\n");
        return targetListenerId;
    }

    // register listener of event feed
    int feedListenerId = obix_registerListener(SENSOR_FLOOR_FEED_CONNECTION,
                         0,
                         "/obix/elsi/Stok/eventFeed/",
                         &eventFeedListener);
    if (feedListenerId < 0)
    {
        log_error("Unable to register update listener for event feed.\n");
        return feedListenerId;
    }

    // TODO workaround for catching FALLEN event
    int taskId = ptask_schedule(_taskThread,
                                &checkEventsTask,
                                NULL,
                                LAST_EVENT_POLL_PERIOD,
                                EXECUTE_INDEFINITE);


    //    testCycle();

    printf("Sensor Floor is successfully registered at the oBIX server\n\n"
           "Press Enter to stop driver...\n");
    // wait for user input
    getchar();

    // remove targets listener (it is automatically done by calling
    // obix_dispose(), but we want to reset target values at the server before
    // exiting, and we need to stop updating values before this
    obix_unregisterListener(SENSOR_FLOOR_CONNECTION, 0, targetListenerId);

    // reset and free all targets
    resetTargets();

    // release all resources allocated by oBIX client library.
    // No need to close connection or unregister listener explicitly - it is
    // done automatically.
    obix_dispose();
    ixmlDocument_free(_deviceData);

    ptask_cancel(_taskThread, taskId, FALSE);
    ptask_dispose(_taskThread, TRUE);

    return 0;
}
