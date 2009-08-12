/* *****************************************************************************
 * Copyright (c) 2009 Andrey Litvinov
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
 * @todo add description
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <ixml_ext.h>
#include <ptask.h>
#include <log_utils.h>
#include <obix_utils.h>
#include "xml_storage.h"
#include "watch.h"

typedef struct PollTaskParams
{
    obixWatch_pollHandler pollHandler;
    oBIX_Watch* watch;
    Response* response;
    const char* uri;
}
PollTaskParams;

const char* OBIX_META_WATCH_UPDATED_YES = "y";
const char* OBIX_META_WATCH_UPDATED_NO  = "n";

const long OBIX_WATCH_LEASE_NO_CHANGE = -1;

// TODO: load this parameter from settings file
static const int MAX_WATCH_COUNT = 50;

static const char* OBIX_META_ATTR_WATCH_TEMPLATE = "wi-%d";

static const char* WATCH_URI_TEMPLATE = "/obix/watchService/watch%d/";
// length of watch uri prefix, but not of the whole template
static const int WATCH_URI_PREFIX_LENGTH = 24;

/**
 * Array for storing Watch objects created by users.
 */
static oBIX_Watch** _watches;
static int _watchesCount;
/** Thread for removing unused watches. */
static Task_Thread* _threadLease;
/** Thread for parking long poll requests. */
static Task_Thread* _threadLongPoll;

int obixWatch_init()
{
    // check whether watches storage is initialized
    if (_watches != NULL)
    {
        log_warning("Watches are already initialized.");
        return -1;
    }

    _watches = (oBIX_Watch**) calloc(MAX_WATCH_COUNT, sizeof(oBIX_Watch*));
    if (_watches == NULL)
    {
        log_error("Unable to allocate memory for Watch cache.");
        return -2;
    }

    _watchesCount = 0;

    // initialize thread which will be used to remove old watch objects
    _threadLease = ptask_init();
    if (_threadLease == NULL)
    {
        return -3;
    }
    // initialize thread which will be used to park long poll requests
    _threadLongPoll = ptask_init();
    if (_threadLongPoll == NULL)
    {
        return -3;
    }

    log_debug("Watches are successfully initialized.");
    return 0;
}

int obixWatch_dispose()
{
    if (_watches == NULL)
    {	// nothing to be done
        return 0;
    }

    int i;
    int error = 0;
    for (i = 0; (_watchesCount > 0) && (i < MAX_WATCH_COUNT); i++)
    {
        if (_watches[i] != NULL)
        {
            error += obixWatch_delete(_watches[i]);
        }
    }

    free(_watches);
    _watches = NULL;

    // stop threads
    if (_threadLease != NULL)
    {
        ptask_dispose(_threadLease, TRUE);
    }
    if (_threadLongPoll != NULL)
    {
        ptask_dispose(_threadLongPoll, TRUE);
    }

    return error;
}

/**
 * Frees memory, allocated for provided watch item.
 * Also removes meta tag associated with that watch item.
 * @param item Watch item which should be freed.
 * @return Next watch item in the list, or NULL if the provided watch item was
 *         at the end of the list.
 */
static oBIX_Watch_Item* obixWatchItem_free(oBIX_Watch_Item* item)
{
    oBIX_Watch_Item* next = item->next;
    free(item->uri);
    if (xmldb_deleteMeta(item->updated) != 0)
    {
        log_error("Unable to delete meta data corresponding to the deleted watch item.");
    }
    free(item);

    return next;
}

static void obixWatchItem_freeRecursive(oBIX_Watch_Item* item)
{
    if (item->next != NULL)
    {
        obixWatchItem_freeRecursive(item->next);
    }

    obixWatchItem_free(item);
}

static int obixWatch_free(oBIX_Watch* watch)
{
    int watchId = watch->id;
    if (watch == NULL)
    {
        return -1;
    }

    if (watch->items != NULL)
    {
        obixWatchItem_freeRecursive(watch->items);
    }

    pthread_cond_destroy(&(watch->pollTaskCompleted));
    pthread_mutex_destroy(&(watch->pollTaskMutex));

    free(watch);
    _watchesCount--;
    _watches[watchId - 1] = NULL;
    return 0;
}

static int obixWatch_deleteHelper(oBIX_Watch* watch)
{
    // remove watch from the storage
    char* watchUri = obixWatch_getUri(watch);
    if (watchUri == NULL)
    {
        return -3;
    }
    int error = xmldb_delete(watchUri);
    free(watchUri);
    if (error != 0)
    {
        return -1;
    }

    // cancel waiting poll request handler if any
    pthread_mutex_lock(&(watch->pollTaskMutex));
    if (watch->pollTaskId > 0)
    {
        int error = ptask_reschedule(_threadLongPoll,
                                     watch->pollTaskId,
                                     0,
                                     1,
                                     FALSE);
        if (error != 0)
        {
            log_error("Unable to call last long poll request handler: "
                      "ptask_reschedule returned %d.", error);
        }
        else
        {
            // wait until the poll handler completes before proceeding
            pthread_cond_wait(&(watch->pollTaskCompleted),
                              &(watch->pollTaskMutex));
        }
    }
    pthread_mutex_unlock(&(watch->pollTaskMutex));

    // clean watch object
    if (obixWatch_free(watch) != 0)
    {
        return -2;
    }

    return 0;
}


int obixWatch_delete(oBIX_Watch* watch)
{
    // remove watch deleting task
    int error = ptask_cancel(_threadLease, watch->leaseTimerId, TRUE);
    if (error != 0)
    {
        log_error("Unable to cancel watch lease timer: "
                  "ptask_cancel() returned %d.", error);
        return -3;
    }

    return obixWatch_deleteHelper(watch);
}

static void taskDeleteWatch(void* arg)
{
    oBIX_Watch* watch =(oBIX_Watch*) arg;
    log_debug("Deleting unused Watch object (#%d).", watch->id);
    int error = obixWatch_deleteHelper(watch);
    if (error != 0)
    {
        log_error("Unable to delete Watch object by timeout: "
                  "obixWatch_delete() returned %d.", error);
    }
}

static char* generateWatchUri(int watchId)
{
    char* watchUri = (char*) malloc(WATCH_URI_PREFIX_LENGTH + 8);
    if (watchUri == NULL)
    {
        return NULL;
    }

    // extra bytes in watchUri are reserved for the 'watchId/\0' ending
    int error = sprintf(watchUri, WATCH_URI_TEMPLATE, watchId);
    if (error < 0)
    {
        log_error("Unable to cerate Watch URI: sprintf returned %d.", error);
        free(watchUri);
        return NULL;
    }

    return watchUri;
}

static long getLeaseTime(IXML_Element* watchDOM)
{
    // find <lease/> tag
    IXML_Document* doc = ixmlNode_getOwnerDocument(
                             ixmlElement_getNode(watchDOM));
    IXML_Element* lease = ixmlDocument_getElementById(doc, OBIX_OBJ_RELTIME);

    if (lease == NULL)
    {
        log_error("Unable to find <lease/> tag in watch stub.");
        return -1;
    }

    // parse lease value
    const char* attrVal = ixmlElement_getAttribute(lease, OBIX_ATTR_VAL);
    if (lease == NULL)
    {
        log_error("<lease/> tag in watch stub object does not "
                  "contain \"%s\" attribute.", OBIX_ATTR_VAL);
        return -1;
    }

    long period;
    int error = obix_reltime_parseToLong(attrVal, &period);
    if (error != 0)
    {
        log_error("<lease/> tag in watch stub object contains "
                  "wrong value: \"%s\".", attrVal);
        return -1;
    }

    return period;
}

int obixWatch_create(IXML_Element** watchDOM)
{
    oBIX_Watch* watch = NULL;
    char* watchUri = NULL;
    IXML_Element* watchElement = NULL;

    // helper method for releasing memory on error
    void cleanup()
    {
        if (watch != NULL)
            free(watch);
        if (watchUri != NULL)
            free(watchUri);
        if (watchElement != NULL)
            ixmlElement_freeOwnerDocument(watchElement);
    }

    if (_watchesCount >= MAX_WATCH_COUNT)
    {
        // all watch slots are busy.
        log_warning("Unable to create new Watch object: "
                    "Maximum objects count is reached.");
        return -2;
    }

    // search for free slot
    int watchId = 0;
    while ((watchId < MAX_WATCH_COUNT) && (_watches[watchId] != NULL))
    {
        watchId++;
    }
    if (watchId == MAX_WATCH_COUNT)
    {
        // this should never ever happen
        log_error("Unable to create new Watch object: "
                  "All watch slots are busy, but they shouldn't.");
        return -4;
    }

    // create new Watch instance
    watch = (oBIX_Watch*) malloc(sizeof(oBIX_Watch));
    // extra bytes in watchUri are reserved for the 'watchId/\0' ending
    watchUri = generateWatchUri(watchId + 1);
    if ((watch == NULL) || (watchUri == NULL))
    {
        log_error("Unable to create new Watch object: "
                  "Not enough memory.");
        cleanup();
        return -1;
    }

    // initialize Watch object
    watch->items = NULL;
    // we start counting watches from 1
    watch->id = watchId + 1;
    // set default poll wait times to 0, which means that pollChanges has
    // standard behavior
    watch->pollWaitMin = 0;
    watch->pollWaitMax = 0;
    // zero value of pollTaskId says that no task was scheduled
    watch->pollTaskId = 0;
    pthread_mutex_init(&(watch->pollTaskMutex), NULL);
    pthread_cond_init(&(watch->pollTaskCompleted), NULL);
    watch->isPollWaitingMax = FALSE;
    // save watch reference
    _watches[watchId] = watch;
    _watchesCount++;

    // create watch object in the storage
    watchElement = xmldb_getObixSysObject(OBIX_SYS_WATCH_STUB);
    if (watchElement == NULL)
    {
        log_error("Unable to create watch object: "
                  "Unable to retrieve watch stub.");
        cleanup();
        return -4;
    }

    // set URI to the watch object
    int error = ixmlElement_setAttributeWithLog(
                    watchElement,
                    OBIX_ATTR_HREF,
                    watchUri);
    if (error != 0)
    {
        cleanup();
        return -4;
    }

    // get lease time for the new watch
    long leaseTime = getLeaseTime(watchElement);
    if (leaseTime <= 0)
    {
        cleanup();
        return -4;
    }

    //save created Watch object in the storage
    char* watchData = ixmlPrintNode(ixmlElement_getNode(watchElement));
    error = xmldb_put(watchData);
    if (watchData != NULL)
    {
        free(watchData);
    }
    if (error != 0)
    {
        cleanup();
        return -3;
    }
    free(watchUri);

    // create task for removing unused watch
    watch->leaseTimerId = ptask_schedule(_threadLease, &taskDeleteWatch, watch, leaseTime, 1);
    if (watch->leaseTimerId < 0)
    {
        log_error("Unable to schedule watch deleting task: "
                  "ptask_schedule() returned %d.", watch->leaseTimerId);
        cleanup();
        return -4;
    }

    *watchDOM = watchElement;

    return watch->id;
}

oBIX_Watch* obixWatch_get(int watchId)
{
    // actual position in array is one less than id
    watchId--;
    if ((watchId < 0) || (watchId >= MAX_WATCH_COUNT) ||
            (_watches[watchId] == NULL))
    {
        log_warning("Requesting for wrong Watch ID.");
        return NULL;
    }

    return _watches[watchId];
}

char* obixWatch_getUri(oBIX_Watch* watch)
{
    return generateWatchUri(watch->id);
}

oBIX_Watch* obixWatch_getByUri(const char* uri)
{
    // check that URI contains correct watch address
    if (!obixWatch_isWatchUri(uri))
    {
        return NULL;
    }

    // trying to extract watchId from the URI
    char* end_ptr;
    int watchId = strtol(uri + WATCH_URI_PREFIX_LENGTH, &end_ptr, 10);
    // check that there is a slash right after the parsed watch Id
    if (*end_ptr != '/')
    {
        log_warning("Trying to get watch with wrong URI: %s", uri);
        return NULL;
    }

    return obixWatch_get(watchId);
}

int obixWatch_createWatchItem(oBIX_Watch* watch, const char* uri, oBIX_Watch_Item** watchItem)
{
    // check that the provided URI was not added earlier
    *watchItem = obixWatch_getWatchItem(watch, uri);
    if (*watchItem != NULL)
    {
        // there is already watch item with the same URI
        return 0;
    }

    // try to find the corresponding object in the storage
    int slashFlag = 0;
    IXML_Element* element = xmldb_getDOM(uri, &slashFlag);
    if (element == NULL)
    {
        // no such element in the storage
        return -1;
    }

    // check that URI is exactly the same as the object has
    // (trailing slash issue)
    // see oBIX spec 1.0-cs-01, paragraph 12.2.1 Watch.add
    if (slashFlag != 0)
    {
        // forbid subscribing
        return -1;
    }

    // check that this is not an <op/> object
    if (strcmp(ixmlElement_getTagName(element), OBIX_OBJ_OP) == 0)
    {
        return -2;
    }

    // add meta info to the object
    char attrName[10];
    int error = sprintf(attrName, OBIX_META_ATTR_WATCH_TEMPLATE, watch->id);
    if (error < 0)
    {
        log_error("Unable to create meta attribute. sprintf() returned %d.",
                  error);
        return -3;
    }

    IXML_Node* metaAttr = xmldb_putMeta(element,
                                        attrName,
                                        OBIX_META_WATCH_UPDATED_NO);
    if (metaAttr == NULL)
    {
        log_error("Unable to save meta attribute.");
        return -3;
    }

    // create WatchItem object
    oBIX_Watch_Item* item = (oBIX_Watch_Item*) malloc(sizeof(oBIX_Watch_Item));
    if (item == NULL)
    {
        xmldb_deleteMeta(metaAttr);
        log_error("Unable to create watch item: Not enough memory.");
        return -3;
    }
    item->uri = (char*) malloc(strlen(uri) + 1);
    if (item->uri == NULL)
    {
        free(item);
        xmldb_deleteMeta(metaAttr);
        log_error("Unable to create watch item: Not enough memory.");
        return -3;
    }
    strcpy(item->uri, uri);
    item->doc = element;
    item->updated = metaAttr;
    item->next = NULL;

    // save created watch item
    obixWatch_appendWatchItem(watch, item);
    *watchItem = item;

    return 0;
}

int obixWatch_appendWatchItem(oBIX_Watch* watch, oBIX_Watch_Item* item)
{
    if ((watch == NULL) || (item == NULL))
    {
        log_error("Wrong parameters passed to obixWatch_appendWatchItem.");
        return -1;
    }

    oBIX_Watch_Item* p = watch->items;

    if (p == NULL)
    {
        // this is the first watch item for provided watch
        watch->items = item;
        return 0;
    }

    // find the last watch item in the list
    while (p->next != NULL)
    {
        p = p->next;
    }
    // append new item to the end
    p->next = item;
    return 0;
}

static int findWatchItem(oBIX_Watch* watch,
                         const char* watchItemUri,
                         oBIX_Watch_Item** item,
                         oBIX_Watch_Item** parent)
{
    if ((watch == NULL) || (watchItemUri == NULL))
    {
        return -2;
    }

    oBIX_Watch_Item* child = watch->items;
    oBIX_Watch_Item* p = NULL;

    // iterate through item list searching for URI match
    while (child != NULL)
    {
        if (strcmp(child->uri, watchItemUri) == 0)
        {
            *item = child;
            if (parent != NULL)
            {
                *parent = p;
            }
            return 0;
        }

        p = child;
        child = child->next;
    }

    // nothing is found
    *item = NULL;
    if (parent != NULL)
    {
        *parent = NULL;
    }
    return -1;
}

int obixWatch_deleteWatchItem(oBIX_Watch* watch, const char* watchItemUri)
{
    oBIX_Watch_Item* item;
    oBIX_Watch_Item* parent;

    int error = findWatchItem(watch, watchItemUri, &item, &parent);

    if (error < 0)
    {
        if (error == -2)
        {
            log_error("Wrong parameters passed to obixWatch_deleteWatchItem.");
        }
        return -1;
    }

    if (parent == NULL)
    {
        // the watch item which we are going to delete is at the top of
        // watch item list
        watch->items = obixWatchItem_free(item);
    }
    else
    {
        // remove watch item from the list
        parent->next = obixWatchItem_free(item);
    }

    return 0;
}

oBIX_Watch_Item* obixWatch_getWatchItem(oBIX_Watch* watch, const char* watchItemUri)
{
    oBIX_Watch_Item* item = NULL;

    int error = findWatchItem(watch, watchItemUri, &item, NULL);
    if (error == -2)
    {
        log_error("Wrong parameters passed to obixWatch_getWatchItem.");
    }

    return item;
}

BOOL obixWatchItem_isUpdated(oBIX_Watch_Item* item)
{
    const char* updated = ixmlNode_getNodeValue(item->updated);

    // TODO handle NULL somehow
    // we compare only the first character so there is no need
    // to use strcmp()
    if (*updated == *OBIX_META_WATCH_UPDATED_YES)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

int obixWatchItem_setUpdated(oBIX_Watch_Item* item, BOOL isUpdated)
{
    const char* newValue = isUpdated ?
                           OBIX_META_WATCH_UPDATED_YES :
                           OBIX_META_WATCH_UPDATED_NO;

    return xmldb_updateMeta(item->updated, newValue);
}

static void obixWatch_notifyPollTask(IXML_Element* meta)
{
    // let's find out what Watch does this meta tag belongs to
    // extract watch id from the meta tag name
    const char* tagName = ixmlElement_getTagName(meta);
    int watchId = strtol(tagName + 3, NULL, 10);
    oBIX_Watch* watch = obixWatch_get(watchId);
    if (watch == NULL)
    {
        log_error("There is no watch corresponding to %s meta tag.",
                  tagName);
        return;
    }

    pthread_mutex_lock(&(watch->pollTaskMutex));
    // if poll response is scheduled to execute with maximum delay, than
    // reduce delay to the minimum
    if ((watch->pollTaskId > 0) && (watch->isPollWaitingMax))
    {
        watch->isPollWaitingMax = FALSE;
        ptask_reschedule(_threadLongPoll,
                         watch->pollTaskId,
                         watch->pollWaitMin - watch->pollWaitMax,
                         1,
                         TRUE);
    }
    pthread_mutex_unlock(&(watch->pollTaskMutex));
}

void obixWatch_updateMeta(IXML_Element* meta)
{
    // iterate through all meta attributes, setting all
    // watch attributes to updated state.
    IXML_Node* metaNode = ixmlNode_getFirstChild(ixmlElement_getNode(meta));

    for ( ;metaNode != NULL; metaNode = ixmlNode_getNextSibling(metaNode))
    {
        IXML_Element* metaElement = ixmlNode_convertToElement(metaNode);
        if (metaElement == NULL)
        {
            // this piece of meta data is not an element - ignore it
            continue;
        }

        const char* value = ixmlElement_getAttribute(metaElement, OBIX_ATTR_VAL);
        // we compare only one char of the value so there is no need
        // to use strcmp()
        if ((value != NULL) && (*value == *OBIX_META_WATCH_UPDATED_NO))
        {
            int error = ixmlElement_setAttribute(metaElement,
                                                 OBIX_ATTR_VAL,
                                                 OBIX_META_WATCH_UPDATED_YES);
            if (error != IXML_SUCCESS)
            {
                log_error("Unable to update meta information. "
                          "Watches will not work properly.");
                // ignore this meta attribute - it is already an error so there
                // is no reason to continue :)
                continue;
            }

            // notify waiting poll task that it can be executed earlier
            obixWatch_notifyPollTask(metaElement);
        }
    }
}

BOOL obixWatch_isWatchUri(const char* uri)
{
    if (strncmp(uri, WATCH_URI_TEMPLATE, WATCH_URI_PREFIX_LENGTH) == 0)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

int obixWatch_resetLeaseTimer(oBIX_Watch* watch, long newPeriod)
{
    int error;
    if (newPeriod >= 0)
    {	// set new lease time and reset timer
        error = ptask_reschedule(_threadLease,
                                 watch->leaseTimerId,
                                 newPeriod,
                                 1,
                                 FALSE);
        if (error != 0)
        {
            log_error("Unable to reschedule Watch lease timer for the watch "
                      "#%d. New lease value is %d.", watch->id, newPeriod);
            return -1;
        }
    }
    else
    {	// reset lease timer
        error = ptask_reset(_threadLease, watch->leaseTimerId);
        if (error != 0)
        {
            log_error("Unable to reset watch lease timer: "
                      "ptask_reset() returned %d.", error);
            return -1;
        }
    }

    return 0;
}

int obixWatch_processTimeUpdates(const char* uri, IXML_Element* element)
{
    oBIX_Watch* watch = obixWatch_getByUri(uri);
    if (watch == NULL)
    {
        // the uri doesn't correspond to any Watch object
        return 1;
    }

    // we assume that input data is correct
    const char* newValue = ixmlElement_getAttribute(element, OBIX_ATTR_VAL);
    long time;
    int error = obix_reltime_parseToLong(newValue, &time);
    if (error != 0)
    {
        // TODO checking that input has correct format should be done
        // for all input values. In that case it would be possible to
        // check for all errors before storing new value to the storage
        log_warning("Unable to parse reltime value \"%s\" for Watch #%d ",
                    newValue, watch->id);
        return -1;
    }

    // we assume that uri is either .../lease, .../pollWaitInterval/min or
    // .../pollWaitInterval/max So we can distinguish them by checking only
    // the last symbol.
    int lastSymbol = strlen(uri) - 1;
    if (uri[lastSymbol] == '/')
    {
        lastSymbol--;
    }

    switch(uri[lastSymbol])
    {
    case 'e': // /lease
        return obixWatch_resetLeaseTimer(watch, time);
        break;
    case 'n': // /pollWaitInterval/min
        {
            // TODO it can be partly omitted if the check is beforehand
            if ((time < 0) || (time > watch->pollWaitMax))
            {
                log_warning("Unable to update watch%d/pollWaitInterval/min: "
                            "Wrong time (%ld) is provided.",
                            watch->id, time);
                return -1;
            }
            watch->pollWaitMin = time;
        }
        break;
    case 'x': // /pollWaitInterval/max
        {
            if (time < watch->pollWaitMin)
            {
                log_warning("Unable to update watch%d/pollWaitInterval/max: "
                            "Wrong time (%ld) is provided.",
                            watch->id, time);
                return -1;
            }
            watch->pollWaitMax = time;
        }
        break;
    }

    return 0;
}

BOOL obixWatch_isLongPoll(oBIX_Watch* watch)
{
    return (watch->pollWaitMax > 0) ? TRUE : FALSE;
}

//This task is scheduled to execute delayed long poll handler
void obixWatch_longPollTask(void* arg)
{
    PollTaskParams* params = (PollTaskParams*) arg;
    pthread_mutex_lock(&(params->watch->pollTaskMutex));
    // 0 poll task id means that it is not scheduled anymore
    params->watch->pollTaskId = 0;
    (*(params->pollHandler))(params->watch, params->response, params->uri);
    // notify that task is completed
    pthread_cond_signal(&(params->watch->pollTaskCompleted));
    pthread_mutex_unlock(&(params->watch->pollTaskMutex));
    free(params);
}

int obixWatch_holdPoll(obixWatch_pollHandler pollHandler,
                       oBIX_Watch* watch,
                       Response* response,
                       const char* uri,
                       BOOL maxWait)
{
    // check whether there is already scheduled poll response for this watch
    pthread_mutex_lock(&(watch->pollTaskMutex));
    if (watch->pollTaskId > 0)
    {
        log_error("Unable to hold Watch poll request: Previous request is not "
                  "yet answered.");
        pthread_mutex_unlock(&(watch->pollTaskMutex));
        return -1;
    }

    // calculate hold time
    long delay = maxWait ? watch->pollWaitMax : watch->pollWaitMin;

    if (delay == 0)
    {
        // we can execute poll handler right now
        (*pollHandler)(watch, response, uri);
        pthread_mutex_unlock(&(watch->pollTaskMutex));
        return 0;
    }

    // check that we are able to hold this request
    if (!(response->canWait))
    {
        pthread_mutex_unlock(&(watch->pollTaskMutex));
        return -2;
    }

    // remember for how long poll is suspended
    watch->isPollWaitingMax = maxWait;

    // schedule execution of the handler
    // create structure which will hold all parameters for poll task
    PollTaskParams* params = (PollTaskParams*) malloc(sizeof(PollTaskParams));
    if (params == NULL)
    {
        log_error("Unable to hold Watch poll request: Not enough memory.");
        pthread_mutex_unlock(&(watch->pollTaskMutex));
        return -1;
    }

    params->pollHandler = pollHandler;
    params->watch = watch;
    params->response = response;
    params->uri = uri;

    watch->pollTaskId = ptask_schedule(_threadLongPoll,
                                       &obixWatch_longPollTask,
                                       (void*) params,
                                       delay,
                                       1);
    pthread_mutex_unlock(&(watch->pollTaskMutex));
    if (watch->pollTaskId < 0)
    {
        log_error("Unable to hold Watch poll request: "
                  "Unable to schedule task.");
        return watch->pollTaskId;
    }

    log_debug("Request handling is suspended for %ld ms.", delay);

    return 0;
}
