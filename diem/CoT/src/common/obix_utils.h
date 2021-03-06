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
 * Contains oBIX keywords (object names, contracts, facets, etc) and some
 * utility functions.
 * For instance, #obix_obj_create and a set of obix_obj_add*Child functions can
 * be used to generate XML oBIX data.
 *
 * The list of keywords is not complete and should be expanded every time when
 * something new is needed. In most cases a common oBIX client application
 * doesn't need to use any of those, because
 * @ref obix_client.h "oBIX Client API" hides all oBIX syntax (except case with
 * registering new device data).
 * @n
 * Read more about oBIX from http://obix.org/
 *
 * @author Andrey Litvinov
 */

#ifndef OBIX_UTILS_H_
#define OBIX_UTILS_H_

#include "ixml_ext.h"

/** @name oBIX Error Contracts' URIs
 * Can be used to define the error type returned by an oBIX server.
 * @{
 */
/** URI of the @a BadUriErr error contract. */
extern const char* OBIX_CONTRACT_ERR_BAD_URI;
/** URI of the @a UnsupportedErr error contract. */
extern const char* OBIX_CONTRACT_ERR_UNSUPPORTED;
/** URI of the @a PermissionErr error contract. */
extern const char* OBIX_CONTRACT_ERR_PERMISSION;
/** @} */

/** @name oBIX Object Types (XML Element Types)
 * @{
 */
/** oBIX Object (@a obj) */
extern const char* OBIX_OBJ;
/** oBIX Reference (@a ref) */
extern const char* OBIX_OBJ_REF;
/** oBIX Operation (@a op) */
extern const char* OBIX_OBJ_OP;
/** oBIX List (@a list) */
extern const char* OBIX_OBJ_LIST;
/** oBIX Error (@a err) */
extern const char* OBIX_OBJ_ERR;
/** oBIX Boolean (@a bool) */
extern const char* OBIX_OBJ_BOOL;
/** oBIX Integer (@a int) */
extern const char* OBIX_OBJ_INT;
/** oBIX Real (@a real) */
extern const char* OBIX_OBJ_REAL;
/** oBIX String (@a str) */
extern const char* OBIX_OBJ_STR;
/** oBIX Enumeration (@a enum) */
extern const char* OBIX_OBJ_ENUM;
/** oBIX Absolute Time (@a abstime) */
extern const char* OBIX_OBJ_ABSTIME;
/** oBIX Relative Duration of Time (@a reltime) */
extern const char* OBIX_OBJ_RELTIME;
/** oBIX URI (@a uri) */
extern const char* OBIX_OBJ_URI;
/** oBIX Feed (@a feed) */
extern const char* OBIX_OBJ_FEED;
/** @} */

/** @name oBIX Object Names
 * Object names (value of @a name attributes), which are used in oBIX contracts.
 * @{
 */
/** Name of @a signUp operation in the Lobby object. */
extern const char* OBIX_NAME_SIGN_UP;
/** Name of @a batch operation in the Lobby object. */
extern const char* OBIX_NAME_BATCH;
/** Name of the Watch Service in the Lobby object. */
extern const char* OBIX_NAME_WATCH_SERVICE;
/** Name of the @a watchService.make operation. */
extern const char* OBIX_NAME_WATCH_SERVICE_MAKE;
/** Name of the @a Watch.add operation. */
extern const char* OBIX_NAME_WATCH_ADD;
/** Name of the @a Watch.addOperation operation. */
extern const char* OBIX_NAME_WATCH_ADD_OPERATION;
/** Name of the @a Watch.operationResponse operation. */
extern const char* OBIX_NAME_WATCH_OPERATION_RESPONSE;
/** Name of the @a Watch.remove operation. */
extern const char* OBIX_NAME_WATCH_REMOVE;
/** Name of the @a Watch.pollChanges operation. */
extern const char* OBIX_NAME_WATCH_POLLCHANGES;
/** Name of the @a Watch.pollRefresh operation. */
extern const char* OBIX_NAME_WATCH_POLLREFRESH;
/** Name of the @a Watch.delete operation. */
extern const char* OBIX_NAME_WATCH_DELETE;
/** Name of the @a Watch.lease parameter. */
extern const char* OBIX_NAME_WATCH_LEASE;
/** Name of the @a Watch.pollWaitInterval object. */
extern const char* OBIX_NAME_WATCH_POLL_WAIT_INTERVAL;
/** Name of the @a Watch.pollWaitInterval.min parameter. */
extern const char* OBIX_NAME_WATCH_POLL_WAIT_INTERVAL_MIN;
/** Name of the @a Watch.pollWaitInterval.max parameter. */
extern const char* OBIX_NAME_WATCH_POLL_WAIT_INTERVAL_MAX;
/** @} */

/** String representation of oBIX @a NULL object. */
extern const char* OBIX_OBJ_NULL_TEMPLATE;

/** @name oBIX Object Attributes and Facets
 * @{
 */
/** Object attribute @a "is". */
extern const char* OBIX_ATTR_IS;
/** Object attribute @a "name". */
extern const char* OBIX_ATTR_NAME;
/** Object attribute @a "href". */
extern const char* OBIX_ATTR_HREF;
/** Object attribute @a "val". */
extern const char* OBIX_ATTR_VAL;
/** Object attribute @a "null". */
extern const char* OBIX_ATTR_NULL;
/** oBIX facet @a "writable". */
extern const char* OBIX_ATTR_WRITABLE;
/** oBIX facet @a "display". */
extern const char* OBIX_ATTR_DISPLAY;
/** oBIX facet @a "displayName". */
extern const char* OBIX_ATTR_DISPLAY_NAME;
/** oBIX facet @a "min". */
extern const char* OBIX_ATTR_MIN;
/** oBIX facet @a "max". */
extern const char* OBIX_ATTR_MAX;
/** oBIX attribute @a "in". Specifies input argument type of the operation.*/
extern const char* OBIX_ATTR_IN;
/** oBIX attribute @a "out". Specifies output argument type of the operation.*/
extern const char* OBIX_ATTR_OUT;
/** oBIX attribute @a "of". */
extern const char* OBIX_ATTR_OF;
/** @} */

/** String representation of boolean @a true value. */
extern const char* XML_TRUE;
/** String representation of boolean @a false value. */
extern const char* XML_FALSE;

/**
 * Specifies a format of @a reltime value, generated by #obix_reltime_fromLong
 */
typedef enum {
    RELTIME_SEC,
    RELTIME_MIN,
    RELTIME_HOUR,
    RELTIME_DAY,
    RELTIME_MONTH,
    RELTIME_YEAR
} RELTIME_FORMAT;

/**
 * Parses string value of @a reltime object and returns corresponding time in
 * milliseconds. Follows @a xs:duration format.
 *
 * @note Durations which can overload @a long variable are not parsed.
 *
 * @param str String value of a @a reltime object which should be parsed.
 * @param duration If parsing is successful than the parsed value will be
 *                 written there.
 * @return @li @a 0 - Operation completed successfully;
 *         @li @a -1 - Parsing error (provided string has bad format);
 *         @li @a -2 - Provided @a reltime value is bigger than or equal to 24
 *                     days (The maximum possible value is
 *                     @a "P23DT23H59M59.999S"). Also this error code is
 *                     returned when the input value is not normalized: If some
 *                     of time components (e.g. hours) presents, than all
 *                     smaller components (minutes and seconds) should represent
 *                     less time than previous component. For example
 *                     @a "PT60M" and @a "PT60S" are allowed, but @a "PT1H60M"
 *                     and @a "PT1H60S" are not.
 */
int obix_reltime_parseToLong(const char* str, long* duration);

/**
 * Generates @a reltime value from the provided time in milliseconds.
 *
 * @param duration Time in milliseconds which should be converted.
 * @param format Format of the generated @a reltime value. Specifies the maximum
 *               time component for the output value. For example, converting
 *               2 minutes with @a RELTIME_MIN will result in @a "PT2M";
 *               @a RELTIME_SEC - @a "PT120S".
 * @return String, which represents provided time in @a xs:duration format, or
 *         @a NULL if memory allocation failed.
 */
char* obix_reltime_fromLong(long duration, RELTIME_FORMAT format);

/**
 * Checks whether oBIX object implements specified contract. Object implements
 * a contract when contract's URI is listed in object's @a is attribute.
 *
 * @param obj XML DOM structure representing an oBIX object.
 * @param contract URI of the contract which should be checked.
 * @return #TRUE if the object implements specified contract, #FALSE otherwise.
 */
BOOL obix_obj_implementsContract(IXML_Element* obj, const char* contract);

/**
 * Checks whether the provided oBIX object is NULL. Object is null if its
 * @a null attribute equals @a true.
 * return #TRUE if the object is NULL, #FALSE otherwise.
 */
BOOL obix_obj_isNull(IXML_Element* obj);

/**
 * Creates a new XML DOM structure containing oBIX object.
 * Only @a type and @a obj are obligatory parameters. Others can be @a NULL.
 *
 * @param type Type of the object to be created (e.g. OBIX_OBJ_INT).
 * @param uri URI of the object (Value for "href" attribute).
 * @param name Value for the "name" attribute of the object.
 * @param displayName Value for the "displayName" attribute of the object.
 * @param doc If not @a NULL, a reference to the created XML document will be
 *            returned here. (Otherwise, this reference can be extracted from
 *            the returned object using ixml utility methods).
 * @param obj Reference to the XML element representing created oBIX object will
 * 			  be returned here.
 * @return @li @a 0 on success;
 * 		   @li @a -2 - Wrong input arguments.
 * 		   @li @a -1 - Other error.
 */
int obix_obj_create(const char* type,
                    const char* uri,
                    const char* name,
                    const char* displayName,
                    IXML_Document** doc,
                    IXML_Element** obj);

/**
 * Sets "href" attribute for the provided oBIX object.
 * The value is created as "<parent.href><uriPart>/".
 *
 * @param parent Parent oBIX object.
 * @param child Child object, whose "href" attribute must be changed.
 * @param uriPart URI piece which would be appended to the parent's URI in
 * 				  order to form "href" attribute for child object.
 * @return @a 0 on success, @a -1 on error.
 */
int obix_obj_setChildUri(IXML_Element* parent,
                         IXML_Element* child,
                         const char* uriPart);

/**
 * Creates an oBIX object with provided parameters and adds it as a child to
 * provided parent object.
 * Only @a parent and @a type are obligatory parameters. Others can be @a NULL.
 *
 * @param parent Parent oBIX object.
 * @param type Type of the object to be created (e.g. OBIX_OBJ_INT).
 * @param uriPart URI part for the created object. It will be appended to
 *                parent's URI in order to create "href" attribute for creating
 *                object.
 * @param name Value for the "name" attribute of the object.
 * @param displayName Value for the "displayName" attribute of the object.
 * @param child If not @a NULL, a reference to the XML element representing
 * 			    created oBIX object will be returned here.
 * @return @a 0 on success, @a -1 on error.
 */
int obix_obj_addChild(IXML_Element* parent,
                      const char* type,
                      const char* uriPart,
                      const char* name,
                      const char* displayName,
                      IXML_Element** child);

/**
 * Creates oBIX @a val (the one with value attribute) object with provided
 * parameters and adds it as a child to provided parent object.
 * Only @a parent and @a type are obligatory parameters. Others can be @a NULL.
 *
 * @param parent Parent oBIX object.
 * @param type Type of the object to be created (e.g. OBIX_OBJ_INT).
 * @param uriPart URI part for the created object. It will be appended to
 *                parent's URI in order to create "href" attribute for creating
 *                object.
 * @param name Value for the "name" attribute of the object.
 * @param displayName Value for the "displayName" attribute of the object.
 * @param value Value for the created object (value of "val" attribute). If this
 *  			parameter is @a NULL, object will have "null" attribute set to
 *  			true.
 * @param writable If #TRUE, created object will have "writable" attribute set
 * 				to true. I.e. it will be possible to modify the value of this
 * 				object.
 * @param child If not @a NULL, a reference to the XML element representing
 * 			    created oBIX object will be returned here.
 * @return @a 0 on success, @a -1 on error.
 */
int obix_obj_addValChild(IXML_Element* parent,
                         const char* type,
                         const char* uriPart,
                         const char* name,
                         const char* displayName,
                         const char* value,
                         BOOL writable,
                         IXML_Element** child);

/**
 * Creates oBIX @a string object with provided parameters and adds it as a
 * child to provided parent object.
 * Similar to call #obix_obj_addValChild with @a type set to #OBIX_OBJ_STR.
 * Only @a parent is obligatory parameter. Others can be @a NULL.
 *
 * @param parent Parent oBIX object.
 * @param uriPart URI part for the created object. It will be appended to
 *                parent's URI in order to create "href" attribute for creating
 *                object.
 * @param name Value for the "name" attribute of the object.
 * @param displayName Value for the "displayName" attribute of the object.
 * @param value Value for the created object (value of "val" attribute). If this
 *  			parameter is @a NULL, object will have "null" attribute set to
 *  			true.
 * @param writable If #TRUE, created object will have "writable" attribute set
 * 				to true. I.e. it will be possible to modify the value of this
 * 				object.
 * @param child If not @a NULL, a reference to the XML element representing
 * 			    created oBIX object will be returned here.
 * @return @a 0 on success, @a -1 on error.
 */
int obix_obj_addStringChild(IXML_Element* parent,
                            const char* uriPart,
                            const char* name,
                            const char* displayName,
                            const char* value,
                            BOOL writable,
                            IXML_Element** child);

/**
 * Creates oBIX @a integer object with provided parameters and adds it as a
 * child to provided parent object.
 * Only @a parent is obligatory parameter. Others can be @a NULL.
 *
 * @param parent Parent oBIX object.
 * @param uriPart URI part for the created object. It will be appended to
 *                parent's URI in order to create "href" attribute for creating
 *                object.
 * @param name Value for the "name" attribute of the object.
 * @param displayName Value for the "displayName" attribute of the object.
 * @param value Value for the created object (value of "val" attribute).
 * @param writable If #TRUE, created object will have "writable" attribute set
 * 				to true. I.e. it will be possible to modify the value of this
 * 				object.
 * @param child If not @a NULL, a reference to the XML element representing
 * 			    created oBIX object will be returned here.
 * @return @a 0 on success, @a -1 on error.
 */
int obix_obj_addIntegerChild(IXML_Element* parent,
                             const char* uriPart,
                             const char* name,
                             const char* displayName,
                             int value,
                             BOOL writable,
                             IXML_Element** child);

/**
 * Creates oBIX @a integer object with provided parameters and adds it as a
 * child to provided parent object.
 * Only @a parent is obligatory parameter. Others can be @a NULL.
 *
 * @param parent Parent oBIX object.
 * @param uriPart URI part for the created object. It will be appended to
 *                parent's URI in order to create "href" attribute for creating
 *                object.
 * @param name Value for the "name" attribute of the object.
 * @param displayName Value for the "displayName" attribute of the object.
 * @param value Value for the created object (value of "val" attribute).
 * @param precision Number of digits after floating point.
 * @param writable If #TRUE, created object will have "writable" attribute set
 * 				to true. I.e. it will be possible to modify the value of this
 * 				object.
 * @param child If not @a NULL, a reference to the XML element representing
 * 			    created oBIX object will be returned here.
 * @return @a 0 on success, @a -1 on error.
 */
int obix_obj_addRealChild(IXML_Element* parent,
                          const char* uriPart,
                          const char* name,
                          const char* displayName,
                          double value,
                          int precision,
                          BOOL writable,
                          IXML_Element** child);

/**
 * Creates oBIX @a boolean object with provided parameters and adds it as a
 * child to provided parent object.
 * Only @a parent is obligatory parameter. Others can be @a NULL.
 *
 * @param parent Parent oBIX object.
 * @param uriPart URI part for the created object. It will be appended to
 *                parent's URI in order to create "href" attribute for creating
 *                object.
 * @param name Value for the "name" attribute of the object.
 * @param displayName Value for the "displayName" attribute of the object.
 * @param value Value for the created object (value of "val" attribute).
 * @param writable If #TRUE, created object will have "writable" attribute set
 * 				to true. I.e. it will be possible to modify the value of this
 * 				object.
 * @param child If not @a NULL, a reference to the XML element representing
 * 			    created oBIX object will be returned here.
 * @return @a 0 on success, @a -1 on error.
 */
int obix_obj_addBooleanChild(IXML_Element* parent,
                             const char* uriPart,
                             const char* name,
                             const char* displayName,
                             BOOL value,
                             BOOL writable,
                             IXML_Element** child);

#endif /* OBIX_UTILS_H_ */
