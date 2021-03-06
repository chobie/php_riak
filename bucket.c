/*
   Copyright 2013 Trifork A/S
   Author: Kaspar Bach Pedersen

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#include "php_riak.h"
#include "bucket.h"
#include "bucket_properties.h"
#include "client.h"
#include "object.h"
#include "exceptions.h"


riak_connection *get_riak_connection(zval *zbucket TSRMLS_DC);

zend_class_entry *riak_bucket_ce;

#define GET_RIAK_CONNECTION_RETURN_EXC_ON_ERROR( VAR ) VAR = get_riak_connection(getThis() TSRMLS_CC); \
  if (!VAR) { \
      zend_throw_exception(riak_badarguments_exception_ce, "No client", 500 TSRMLS_CC); \
      return; \
  } 

#define COPY_CLIENT_LONG_PROPERTY_IF_SET( CLIENT, NAME, ZP ) \
    ZP = zend_read_property(riak_client_ce, CLIENT, NAME, sizeof(NAME)-1, 1 TSRMLS_CC); \
    if (ZP && Z_TYPE_P(ZP) == IS_LONG) { \
        zend_update_property_long(riak_bucket_ce, getThis(), NAME, sizeof(NAME)-1, Z_LVAL_P(ZP) TSRMLS_CC); \
    }

#define SET_LONG_PROPERTY_AS_RIACK_MEMBER_IF_PRESENT(NAME, MUSE, MVAL, ZP) \
    ZP = zend_read_property(riak_bucket_ce, getThis(), NAME, sizeof(NAME)-1, 1 TSRMLS_CC); \
    if (Z_TYPE_P(ZP) == IS_LONG) { \
        MUSE = 1; \
        MVAL = Z_LVAL_P(ZP); \
    }

ZEND_BEGIN_ARG_INFO_EX(arginfo_bucket_ctor, 0, ZEND_RETURN_VALUE, 2)
    ZEND_ARG_INFO(0, client)
    ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_bucket_put, 0, ZEND_RETURN_VALUE, 1)
    ZEND_ARG_INFO(0, object)
    ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_bucket_get, 0, ZEND_RETURN_VALUE, 1)
    ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_bucket_delete, 0, ZEND_RETURN_VALUE, 1)
    ZEND_ARG_INFO(0, object)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_bucket_fetchprops, 0, ZEND_RETURN_VALUE, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_bucket_applyprops, 0, ZEND_RETURN_VALUE, 1)
	ZEND_ARG_INFO(0, bucket_properties)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_bucket_indexq, 0, ZEND_RETURN_VALUE, 2)
    ZEND_ARG_INFO(0, index)
    ZEND_ARG_INFO(0, from)
    ZEND_ARG_INFO(0, to)
ZEND_END_ARG_INFO()

static zend_function_entry riak_bucket_methods[] = {
	PHP_ME(RiakBucket, __construct, arginfo_bucket_ctor, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(RiakBucket, putObject, arginfo_bucket_put, ZEND_ACC_PUBLIC)
	PHP_ME(RiakBucket, getObject, arginfo_bucket_get, ZEND_ACC_PUBLIC)
	PHP_ME(RiakBucket, deleteObject, arginfo_bucket_delete, ZEND_ACC_PUBLIC)
    PHP_ME(RiakBucket, indexQuery, arginfo_bucket_indexq, ZEND_ACC_PUBLIC)

	PHP_ME(RiakBucket, fetchProperties, arginfo_bucket_fetchprops, ZEND_ACC_PUBLIC)
	PHP_ME(RiakBucket, applyProperties, arginfo_bucket_applyprops, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};

void riak_bucket_init(TSRMLS_D) /* {{{ */
{
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "RiakBucket", riak_bucket_methods);
	riak_bucket_ce = zend_register_internal_class(&ce TSRMLS_CC);

	zend_declare_property_null(riak_bucket_ce, "name", sizeof("name")-1, ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(riak_bucket_ce, "client", sizeof("client")-1, ZEND_ACC_PUBLIC TSRMLS_CC);

	zend_declare_property_null(riak_bucket_ce, "nVal", sizeof("nVal")-1, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_null(riak_bucket_ce, "allowMult", sizeof("allowMult")-1, ZEND_ACC_PUBLIC TSRMLS_CC);

    zend_declare_property_null(riak_bucket_ce, "w", sizeof("w")-1, ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(riak_bucket_ce, "dw", sizeof("dw")-1, ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(riak_bucket_ce, "pw", sizeof("pw")-1, ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(riak_bucket_ce, "r", sizeof("r")-1, ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(riak_bucket_ce, "rw", sizeof("rw")-1, ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(riak_bucket_ce, "pr", sizeof("pr")-1, ZEND_ACC_PUBLIC TSRMLS_CC);
}
/* }}} */

zval* create_bucket_object(zval* zclient, char* name TSRMLS_DC) /* {{{ */
{
	zval *zbucket, *zname;
	MAKE_STD_ZVAL(zbucket);
	MAKE_STD_ZVAL(zname);

	ZVAL_STRING(zname, name, 1);

	object_init_ex(zbucket, riak_bucket_ce);
	CALL_METHOD2(RiakBucket, __construct, zbucket, zbucket, zclient, zname);

	zval_ptr_dtor(&zname);
	return zbucket;
}
/* }}} */

/* {{{ proto void RiakBucket->__construct(RiakClient $client, string $name)
Create a new RiakBucket */
PHP_METHOD(RiakBucket, __construct)
{
	char *name;
	int nameLen;
    zval* client, *zprop;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "os", &client, &name, &nameLen) == FAILURE) {
		return;
	}
	zend_update_property_stringl(riak_bucket_ce, getThis(), "name", sizeof("name")-1, name, nameLen TSRMLS_CC);
	zend_update_property(riak_bucket_ce, getThis(), "client", sizeof("client")-1, client TSRMLS_CC);

    /* Get default w, dw, pw, r, rw and pr from client */
    COPY_CLIENT_LONG_PROPERTY_IF_SET( client, "w", zprop)
    COPY_CLIENT_LONG_PROPERTY_IF_SET( client, "dw", zprop)
    COPY_CLIENT_LONG_PROPERTY_IF_SET( client, "pw", zprop)
    COPY_CLIENT_LONG_PROPERTY_IF_SET( client, "r", zprop)
    COPY_CLIENT_LONG_PROPERTY_IF_SET( client, "rw", zprop)
    COPY_CLIENT_LONG_PROPERTY_IF_SET( client, "w", zprop)
}
/* }}} */


/* {{{ proto array RiakBucket->indexQuery(string $index, string $from [, string $to])
Apply given properties to this bucket */
PHP_METHOD(RiakBucket, indexQuery)
{
    RIACK_STRING rsbucket, rsindex, rsfrom, rsto;
    RIACK_STRING_LIST resultlist;
    char *index, *from, *to;
    int indexlen, fromlen, tolen, riackstatus, i;
    riak_connection *connection;
    zval *zresult;
    tolen = 0; to = NULL;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|s", &index, &indexlen, &from, &fromlen, &to, &tolen) == FAILURE) {
        return;
    }

    GET_RIAK_CONNECTION_RETURN_EXC_ON_ERROR(connection)
    rsbucket = riack_name_from_bucket(getThis() TSRMLS_CC);
    rsindex.len = indexlen;
    rsindex.value = index;
    rsfrom.len = fromlen;
    rsfrom.value = from;
    if (to != NULL && tolen > 0) {
        rsto.len = tolen;
        rsto.value = to;
        riackstatus = riack_2i_query_range(connection->client, rsbucket, rsindex, rsfrom, rsto, &resultlist);
    } else {
        riackstatus = riack_2i_query_exact(connection->client, rsbucket, rsindex, rsfrom, &resultlist);
    }
    CHECK_RIACK_STATUS_THROW_AND_RETURN_ON_ERROR(connection, riackstatus);

    MAKE_STD_ZVAL(zresult);
    array_init(zresult);
    for (i=0; i<resultlist.string_count; ++i) {
        add_next_index_stringl(zresult, resultlist.strings[i].value, resultlist.strings[i].len, 1);
    }
    riack_free_string_list(connection->client, &resultlist);
    RETURN_ZVAL(zresult, 0, 1);
}
/* }}} */

/* {{{ proto void RiakBucket->applyProperties(RiakBucketProperties $properties)
Apply given properties to this bucket */
PHP_METHOD(RiakBucket, applyProperties)
{
	riak_connection *connection;

	RIACK_STRING bucketName;
	zval* zBucketPropsObj, **zTmp;
	HashTable *htBucketPropsProps;
	int riackResult;
	uint32_t nVal;
	uint8_t allowMult;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "o", &zBucketPropsObj) == FAILURE) {
		return;
	}
	GET_RIAK_CONNECTION_RETURN_EXC_ON_ERROR(connection)
    bucketName = riack_name_from_bucket(getThis() TSRMLS_CC);
	htBucketPropsProps = zend_std_get_properties(zBucketPropsObj TSRMLS_CC);

	if ((zend_hash_find(htBucketPropsProps, "nVal", sizeof("nVal"), (void**)&zTmp) == SUCCESS)
     	&& (Z_TYPE_P(*zTmp) == IS_LONG)) {
		nVal = (uint32_t)Z_LVAL_PP(zTmp);
 	} else {
 		zend_throw_exception(riak_badarguments_exception_ce, "nVal must be integer", 5000 TSRMLS_CC);
 		return;
 	}
 	if ((zend_hash_find(htBucketPropsProps, "allowMult", sizeof("allowMult"), (void**)&zTmp) == SUCCESS)
     	&& (Z_TYPE_P(*zTmp) == IS_BOOL)) {
		allowMult = Z_BVAL_PP(zTmp);
 	} else {
 		zend_throw_exception(riak_badarguments_exception_ce, "allowMult must be boolean", 5001 TSRMLS_CC);
 		return;
 	}
 	riackResult = riack_set_bucket_props(connection->client, bucketName, nVal, allowMult);
 	CHECK_RIACK_STATUS_THROW_AND_RETURN_ON_ERROR(connection, riackResult);
}
/* }}} */

/* {{{ proto RiakBucketProperties RiakBucket->fetchProperties()
Fetch and return a RiakBucketProperties object with properties for this bucket */
PHP_METHOD(RiakBucket, fetchProperties)
{
	riak_connection *connection;
	RIACK_STRING bucketName;
	uint32_t nVal = 3;
	uint8_t allowMult = 0;
	int riackResult;
	zval *zBucketProps, *zAllowMult, *zNVal;

	GET_RIAK_CONNECTION_RETURN_EXC_ON_ERROR(connection)

    bucketName = riack_name_from_bucket(getThis() TSRMLS_CC);
	riackResult = riack_get_bucket_props(connection->client,  bucketName, &nVal, &allowMult);

	CHECK_RIACK_STATUS_THROW_AND_RETURN_ON_ERROR(connection, riackResult);

	MAKE_STD_ZVAL(zNVal);
	ZVAL_LONG(zNVal, nVal);

	MAKE_STD_ZVAL(zAllowMult);
	ZVAL_BOOL(zAllowMult, allowMult);

	MAKE_STD_ZVAL(zBucketProps);
	object_init_ex(zBucketProps, riak_bucket_properties_ce);
	CALL_METHOD2(RiakBucketProperties, __construct, zBucketProps, zBucketProps, zNVal, zAllowMult);
	RETVAL_ZVAL(zBucketProps, 0, 1);

	zval_ptr_dtor(&zNVal);
	zval_ptr_dtor(&zAllowMult);
}
/* }}} */

/* {{{ proto void RiakBucket->deleteObject(RiakObject $object)
Deletes given object from riak */
PHP_METHOD(RiakBucket, deleteObject)
{
	struct RIACK_DEL_PROPERTIES props;
	riak_connection *connection;
	zval *zObject, *zTmp;
	RIACK_STRING bucketName, key;
	int riackResult;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "o", &zObject) == FAILURE) {
		return;
	}

	GET_RIAK_CONNECTION_RETURN_EXC_ON_ERROR(connection)

    /* Set bucket name */
    bucketName = riack_name_from_bucket(getThis() TSRMLS_CC);
    /* Set key */
	HASH_GET_INTO_RIACK_STRING_OR_ELSE(riak_object_ce, zObject, "key", zTmp, key) {
		zend_throw_exception(riak_badarguments_exception_ce, "key missing from object", 5001 TSRMLS_CC);
 		return;
	}
	memset(&props, 0, sizeof(props));
    SET_LONG_PROPERTY_AS_RIACK_MEMBER_IF_PRESENT("w", props.w_use, props.w, zTmp)
    SET_LONG_PROPERTY_AS_RIACK_MEMBER_IF_PRESENT("dw", props.dw_use, props.dw, zTmp)
    SET_LONG_PROPERTY_AS_RIACK_MEMBER_IF_PRESENT("pw", props.pw_use, props.pw, zTmp)
    SET_LONG_PROPERTY_AS_RIACK_MEMBER_IF_PRESENT("r", props.r_use, props.r, zTmp)
    SET_LONG_PROPERTY_AS_RIACK_MEMBER_IF_PRESENT("pr", props.pr_use, props.pr, zTmp)
    SET_LONG_PROPERTY_AS_RIACK_MEMBER_IF_PRESENT("rw", props.rw_use, props.rw, zTmp)
    /* vclock ? */
	riackResult = riack_delete(connection->client, bucketName, key, &props);
	CHECK_RIACK_STATUS_THROW_AND_RETURN_ON_ERROR(connection, riackResult);
}
/* }}} */

/* {{{ proto void RiakBucket->putObject(RiakObject $object [, string $key])
Store a RiakObject in riak, if something goes wrong an RiakException is thrown */
PHP_METHOD(RiakBucket, putObject)
{
    char *key;
    int keyLen;
	zval *zObject, *zTmp;
	struct RIACK_OBJECT obj, returnedObj;
	struct RIACK_CONTENT riackContent;
	struct RIACK_PUT_PROPERTIES props;
	riak_connection *connection;
	int riackResult;
	
	key = NULL;
	keyLen = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "o|s", &zObject, &key, &keyLen) == FAILURE) {
		return;
	}
	GET_RIAK_CONNECTION_RETURN_EXC_ON_ERROR(connection)

	memset(&obj, 0, sizeof(obj));
	memset(&returnedObj, 0, sizeof(returnedObj));
	memset(&riackContent, 0, sizeof(riackContent));
	memset(&props, 0, sizeof(props));

    SET_LONG_PROPERTY_AS_RIACK_MEMBER_IF_PRESENT("w", props.w_use, props.w, zTmp)
    SET_LONG_PROPERTY_AS_RIACK_MEMBER_IF_PRESENT("dw", props.dw_use, props.dw, zTmp)
    SET_LONG_PROPERTY_AS_RIACK_MEMBER_IF_PRESENT("pw", props.pw_use, props.pw, zTmp)

    /* fill content */
	set_riak_content_from_object(&riackContent, zObject, connection->client TSRMLS_CC);

    /* fill obj */
    /* Set vclock */
	zTmp = zend_read_property(riak_object_ce, zObject, "vclock", sizeof("vclock")-1, 1 TSRMLS_CC);
	if (Z_TYPE_P(zTmp) == IS_STRING) {
		obj.vclock.len = Z_STRLEN_P(zTmp);
		obj.vclock.clock = (uint8_t*)Z_STRVAL_P(zTmp);
	}
    /* Set bucket name */
    obj.bucket = riack_name_from_bucket(getThis() TSRMLS_CC);
	obj.content_count = 1;
	obj.content = &riackContent;

	if (key) {
        /* Key was passed to function use it. */
		obj.key.len = keyLen;
		obj.key.value = key;
	} else {
        /* No ket provided on function call, get it from RiakObject */
		HASH_GET_INTO_RIACK_STRING_OR_ELSE(riak_object_ce, zObject, "key", zTmp, obj.key) {
            /* Handle possible errors */
		}
	}
	riackResult = riack_put(connection->client, obj, &returnedObj, &props);
	CHECK_RIACK_STATUS_THROW_AND_RETURN_ON_ERROR(connection, riackResult);

    /* TODO Return updated object */
	riack_free_object(connection->client, &returnedObj);
}
/* }}} */

/* {{{ proto RiakObject RiakBucket->getObject(string $key)
Retrieve a RiakObject from riak */
PHP_METHOD(RiakBucket, getObject)
{
	char *key;
    int keyLen, riackResult;
	size_t contentCount;
    zval *zKey, *zVclock, *zExc, *zObjArr, *zObj;
	struct RIACK_GET_PROPERTIES props;
	struct RIACK_GET_OBJECT getResult;
	RIACK_STRING rsBucket, rsKey;
	riak_connection *connection;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &key, &keyLen) == FAILURE) {
		return;
	}
	
	GET_RIAK_CONNECTION_RETURN_EXC_ON_ERROR(connection)

	MAKE_STD_ZVAL(zKey);
	ZVAL_STRINGL(zKey, key, keyLen, 1);

	memset(&props, 0, sizeof(props));
	memset(&getResult, 0, sizeof(getResult));

    SET_LONG_PROPERTY_AS_RIACK_MEMBER_IF_PRESENT("pr", props.pr_use, props.pr, zObj)
    SET_LONG_PROPERTY_AS_RIACK_MEMBER_IF_PRESENT("r", props.r_use, props.r, zObj)

    rsBucket = riack_name_from_bucket(getThis() TSRMLS_CC);
	rsKey.len = keyLen;
	rsKey.value = key;
	riackResult = riack_get(connection->client, rsBucket, rsKey, &props, &getResult);

	if (riackResult == RIACK_SUCCESS) {
		MAKE_STD_ZVAL(zVclock);
		ZVAL_STRINGL(zVclock, (char*)getResult.object.vclock.clock, getResult.object.vclock.len, 1);
		contentCount = getResult.object.content_count;
		if (contentCount > 1) {
			MAKE_STD_ZVAL(zObjArr);
			MAKE_STD_ZVAL(zExc);
			
			array_init(zObjArr);
			while (contentCount > 0) {
				zObj = object_from_riak_content(zKey, &getResult.object.content[--contentCount] TSRMLS_CC);
				add_next_index_zval(zObjArr, zObj);
			}
			object_init_ex(zExc, riak_conflicted_object_exception_ce);
			CALL_METHOD2(RiakConflictedObjectException, __construct, zExc, zExc, zVclock, zObjArr);
			zend_throw_exception_object(zExc TSRMLS_CC);
			
			zval_ptr_dtor(&zObjArr);
		} else if (contentCount == 1) {
			// Create a new RiakObject
			zObj = object_from_riak_content(zKey, &getResult.object.content[0] TSRMLS_CC);
			// Update vclock on RiakObject
			zend_update_property(riak_object_ce, zObj, "vclock", sizeof("vclock")-1, zVclock TSRMLS_CC);
			RETVAL_ZVAL(zObj, 0, 1);
		} else {
			// Throw not found exception
            zend_throw_exception(riak_not_found_exception_ce, "Not Found", 2000 TSRMLS_CC);
		}
		zval_ptr_dtor(&zVclock);
		riack_free_get_object(connection->client, &getResult);
	} else {
		zval_ptr_dtor(&zKey);
		CHECK_RIACK_STATUS_THROW_AND_RETURN_ON_ERROR(connection, riackResult);
	}
	zval_ptr_dtor(&zKey);
}
/* }}} */

zval* object_from_riak_content(zval* key, struct RIACK_CONTENT* content TSRMLS_DC)/* {{{ */
{
	zval *object;
	MAKE_STD_ZVAL(object);
	object_init_ex(object, riak_object_ce);
	CALL_METHOD1(RiakObject, __construct, object, object, key);

	set_object_from_riak_content(object, content TSRMLS_CC);

	return object;
}
/* }}} */

void riak_name_from_bucket(zval* bucket, char **name, int *namelen TSRMLS_DC)/* {{{ */
{
    zval *zname = zend_read_property(riak_bucket_ce, bucket, "name", sizeof("name")-1, 1 TSRMLS_CC);
    if (Z_TYPE_P(zname) == IS_STRING) {
        *name = Z_STRVAL_P(zname);
        *namelen = Z_STRLEN_P(zname);
    } else {
        *name = NULL;
        *namelen = 0;
    }
}
/* }}} */

RIACK_STRING riack_name_from_bucket(zval* bucket TSRMLS_DC)/* {{{ */
{
    RIACK_STRING bucketName;
    int namelen;
    riak_name_from_bucket(bucket, &bucketName.value, &namelen TSRMLS_CC);
    bucketName.len = namelen;
    return bucketName;
}
/* }}} */

riak_connection *get_riak_connection(zval *zbucket TSRMLS_DC)/* {{{ */
{
	zval **zTmp;
	HashTable *zBucketProps;
	riak_connection *connection;
	zBucketProps = zend_std_get_properties(zbucket TSRMLS_CC);
	if (zend_hash_find(zBucketProps, "client", sizeof("client"), (void**)&zTmp) == SUCCESS) {
		GET_RIAK_CONNECTION(*zTmp, connection);
		ensure_connected(connection TSRMLS_CC);
	} else {
		return NULL;
	}
	return connection;
}
/* }}} */

