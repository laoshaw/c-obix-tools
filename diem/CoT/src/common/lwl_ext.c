/** @file
 * Contains implementation of logging tools
 * which extend functionality of  @a liblwl.
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <xml_config.h>
#include <lwl.h>
#include "lwl_ext.h"

const char* CT_LOG = "log";
const char* CT_LOG_LEVEL = "level";
const char* CTAV_LOG_LEVEL_DEBUG = "debug";
const char* CTAV_LOG_LEVEL_WARNING = "warning";
const char* CTAV_LOG_LEVEL_ERROR = "error";
const char* CTAV_LOG_LEVEL_NO = "no";
const char* CT_LOG_FILE = "file";
const char* CT_LOG_FORMAT = "format";
const char* CTA_LOG_PREFIX = "prefix";
const char* CTA_LOG_DATE = "date";
const char* CTA_LOG_TIME = "time";
const char* CTA_LOG_LOCALE = "locale";
const char* CTA_LOG_PRIORITY = "priority";
const char* CT_LOG_NO_FLUSH = "no-flush";

typedef void (*log_function)(lwl_priority_t priority, char* fmt, va_list args);

static void logWithPrintf(lwl_priority_t priority, char* fmt, va_list args);
static void logWithLwl(lwl_priority_t priority, char* fmt, va_list args);

static log_function logFunction = &logWithPrintf;

static lwlh_t logHandle = NULL;

int log_config(IXML_Element* configTag)
{
    IXML_Element* logTag = config_getChildTag(configTag, CT_LOG, TRUE);
    if (logTag == NULL)
        return -1;

    //get the log level.
    lwl_priority_t logLevel;
    IXML_Element* tempTag = config_getChildTag(logTag, CT_LOG_LEVEL, TRUE);
    if (tempTag == NULL)
        return -1;

    const char* tempStr = config_getTagAttributeValue(tempTag, CTA_VALUE, TRUE);
    if (tempStr == NULL)
        return -1;

    if (!strcmp(tempStr, CTAV_LOG_LEVEL_DEBUG))
    {
        logLevel = LWL_PRI_DEBUG;
    }
    else if (!strcmp(tempStr, CTAV_LOG_LEVEL_WARNING))
    {
        logLevel = LWL_PRI_WARNING;
    }
    else if (!strcmp(tempStr, CTAV_LOG_LEVEL_ERROR))
    {
        logLevel = LWL_PRI_ERR;
    }
    else if (!strcmp(tempStr, CTAV_LOG_LEVEL_NO))
    {
        logLevel = LWL_PRI_EMERG;
    }
    else
    {
        log_error("Wrong log level value provided. Available values: "
                 "\"debug\", \"warning\", \"error\" and \"no\".");
        return -1;
    }

    //get the log file
    //default value is stdout
    FILE* logFile = stdout;
    tempTag = config_getChildTag(logTag, CT_LOG_FILE, FALSE);
    if (tempTag != NULL)
    {
        tempStr = config_getTagAttributeValue(tempTag, CTA_VALUE, TRUE);
        if (tempStr == NULL)
            return -1;

        logFile = fopen(tempStr, "a");
        if (logFile == NULL)
        {
            log_error("Unable to open the log file \"%s\".", tempStr);
            return -1;
        }
    }

    //get the log format
    int logOptions = LWL_OPT_NONE;
    const char* logPrefix = NULL;
    tempTag = config_getChildTag(logTag, CT_LOG_FORMAT, FALSE);
    if (tempTag != NULL)
    {
        //prefix for each output message
        logPrefix = config_getTagAttributeValue(tempTag, CTA_LOG_PREFIX, FALSE);
        if (logPrefix != NULL)
        {
            logOptions |= LWL_OPT_PREFIX;
        }

        //print date?
        if (config_getTagBoolAttrValue(tempTag, CTA_LOG_DATE, FALSE))
        {
            logOptions |= LWL_OPT_DATE;
        }
        //print time?
        if (config_getTagBoolAttrValue(tempTag, CTA_LOG_TIME, FALSE))
        {
            logOptions |= LWL_OPT_TIME;
        }
        //print priority of the message?
        if (config_getTagBoolAttrValue(tempTag, CTA_LOG_PRIORITY, FALSE))
        {
            logOptions |= LWL_OPT_PRIORITY;
        }
        //print time and date using current locale format?
        if (config_getTagBoolAttrValue(tempTag, CTA_LOG_LOCALE, FALSE))
        {
            logOptions |= LWL_OPT_USE_LOCALE;
        }
    }

    //Flush the log file everytime after logging?
    tempTag = config_getChildTag(logTag, CT_LOG_NO_FLUSH, FALSE);
    if (tempTag != NULL)
    {
        logOptions |= LWL_OPT_NO_FLUSH;
    }

    //init log handler with extracted parameters
    lwlh_t tempHandle = lwl_alloc();

    if (logPrefix != NULL)
    {
        lwl_set_attributes (tempHandle,
                            LWL_TAG_PREFIX, logPrefix,
                            LWL_TAG_OPTIONS, logOptions,
                            LWL_TAG_FILE, logFile,
                            LWL_TAG_LEVEL, logLevel,
                            LWL_TAG_DONE);
    }
    else
    {
        lwl_set_attributes (tempHandle,
                            LWL_TAG_OPTIONS, logOptions,
                            LWL_TAG_FILE, logFile,
                            LWL_TAG_LEVEL, logLevel,
                            LWL_TAG_DONE);
    }

    logHandle = tempHandle;
    logFunction = &logWithLwl;

    log_debug("\n"
             "--------------------------------------------------------------------------------\n"
             "--------------                Starting new log ...                --------------\n"
             "--------------------------------------------------------------------------------");

    return 0;
}

/**
 * Writes log to @a stdout.
 * It is used when log is not initialized.
 */
static void logWithPrintf(lwl_priority_t priority, char* fmt, va_list args)
{
    switch(priority)
    {
    case LWL_PRI_DEBUG:
        printf("DEBUG ");
        break;
    case LWL_PRI_WARNING:
        printf("WARNING ");
        break;
    case LWL_PRI_ERR:
    default:
        printf("ERROR ");
        break;
    }
    vprintf(fmt, args);
    printf("\n");
}

/**
 * Writes log using @a lwl log handler.
 */
static void logWithLwl(lwl_priority_t priority, char* fmt, va_list args)
{
    int error = -1;
    int bufferSize = 128;
    char* buffer = NULL;

    // quite ugly code, but it is the only way to deal
    // with portability of vsnprintf()
    // see http://perfec.to/vsnprintf/
    while (1)
    {
        buffer = (char*) realloc(buffer, bufferSize);
        error = vsnprintf(buffer, bufferSize, fmt, args);

        if ((error < 0) || (error == bufferSize) || (error == (bufferSize - 1)))
        {
            // error happened but have no clue on how big buffer is needed
            bufferSize = bufferSize << 1;
        }
        else if (error > bufferSize)
        {
            // we know required size
            bufferSize = error + 2;
        }
        else
        {
            break;
        }
    }

    lwl_write_log(logHandle, priority, buffer);
    free(buffer);
}

void log_debug(char* fmt, ...)
{
    va_list args;
    va_start(args,fmt);
    (*logFunction)(LWL_PRI_DEBUG, fmt, args);
    va_end(args);
}

void log_warning(char* fmt, ...)
{
    va_list args;
    va_start(args,fmt);
    (*logFunction)(LWL_PRI_WARNING, fmt, args);
    va_end(args);
}

void log_error(char* fmt, ...)
{
    va_list args;
    va_start(args,fmt);
    (*logFunction)(LWL_PRI_ERR, fmt, args);
    va_end(args);
}

void log_dispose()
{
    if (logHandle != NULL)
    {
        lwl_free(logHandle);
        logHandle = NULL;
        logFunction = &logWithPrintf;
    }
}
