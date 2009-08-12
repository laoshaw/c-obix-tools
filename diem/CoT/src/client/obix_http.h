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
 * @todo add description here
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#ifndef OBIX_HTTP_H_
#define OBIX_HTTP_H_

#include <pthread.h>
#include <table.h>
#include <obix_comm.h>

typedef struct _Http_Connection
{
    Connection c;
    // type specific connection properties
    char* serverUri;
    int serverUriLength;
    char* lobbyUri;
    long pollInterval;
    long watchLease;
    long pollWaitMin;
    long pollWaitMax;

    char* signUpUri;
    char* batchUri;
    char* watchMakeUri;
    char* watchPollChangesFullUri;
    char* watchAddUri;
    char* watchRemoveUri;
    char* watchDeleteUri;

    Table* watchTable;
    pthread_mutex_t watchMutex;
    int watchPollTaskId;

    //	char*
    // is used to delete Watch if there is nothing to monitor
}
Http_Connection;

typedef struct _Http_Device
{
    Device d;
    // type specific device properties
    char* uri;
    int uriLength;
}
Http_Device;

extern const Comm_Stack OBIX_HTTP_COMM_STACK;

/**
 * Subsequent calls have no effect
 */
int http_init();

/**
 * Calls when HTTP stack is not initialized have no effect.
 */
int http_dispose();

int http_initConnection(IXML_Element* connItem,
                        Connection** connection);
int http_openConnection(Connection* connection);
int http_closeConnection(Connection* connection);
void http_freeConnection(Connection* connection);
int http_registerDevice(Connection* connection,
                        Device** device,
                        const char* data);
int http_unregisterDevice(Connection* connection,
                          Device* device);
int http_registerListener(Connection* connection,
                          Device* device,
                          Listener** listener);
int http_unregisterListener(Connection* connection,
                            Device* device,
                            Listener* listener);
int http_readValue(Connection* connection,
                        Device* device,
                        const char* paramUri,
                        char** output);
int http_read(Connection* connection,
                   Device* device,
                   const char* paramUri,
                   IXML_Element** output);
int http_writeValue(Connection* connection,
                    Device* device, const
                    char* paramUri,
                    const char* newValue,
                    OBIX_DATA_TYPE dataType);

int http_sendBatch(oBIX_Batch* batch);

#endif /* OBIX_HTTP_H_ */
