/*
   Copyright 2012 Trifork A/S
   Author: Kaspar Pedersen

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

#include "riak_session.h"

#ifdef PHP_SESSION
#include <zend_exceptions.h>
#include "client.h"
#include "bucket.h"
#include "object.h"
#include "SAPI.h"
#include <ext/standard/php_string.h>
#include <ext/standard/url.h>
#include <main/php_variables.h>

#define SET_CLIENT_OPTION_IF_PRESENT(NAME, CLIENT, ZARR, ZPP) \
    if (zend_hash_find(Z_ARRVAL_P(ZARR), NAME, sizeof(NAME), (void**)&ZPP) == SUCCESS) { \
        convert_to_long(*ZPP); \
        zend_update_property(riak_client_ce, CLIENT, NAME, sizeof(NAME)-1, *ZPP TSRMLS_CC); \
    }

ps_module ps_mod_riak = {
	PS_MOD(riak)
};

typedef struct _riak_session_data {
   char* session_name;
   zval *zbucket;
   zval *zclient;
} riak_session_data;

#define PS_RIAK_DATA riak_session_data *data = PS_GET_MOD_DATA()

PS_OPEN_FUNC(riak) /* {{{ */
{
    riak_session_data* session_data;
    zval *zclient, *zbucket, *zoptionsarray, **zoption;
    php_url *purl;
    char* stripped_path;

    purl = php_url_parse(save_path);
    if (!purl) {
        return FAILURE;
    }

    zclient = create_client_object(purl->host, purl->port TSRMLS_CC);
    if (EG(exception)) {
        php_url_free(purl);
        zval_ptr_dtor(&zclient);
        PS_SET_MOD_DATA(NULL);
        return FAILURE;
    }
    /* Set w,dw,pw,r,rw,pw if set */
    MAKE_STD_ZVAL(zoptionsarray);
    array_init(zoptionsarray);
    if (purl->query) {
        sapi_module.treat_data(PARSE_STRING, estrdup(purl->query), zoptionsarray TSRMLS_CC);
        SET_CLIENT_OPTION_IF_PRESENT("w", zclient, zoptionsarray, zoption)
        SET_CLIENT_OPTION_IF_PRESENT("dw", zclient, zoptionsarray, zoption)
        SET_CLIENT_OPTION_IF_PRESENT("pw", zclient, zoptionsarray, zoption)
        SET_CLIENT_OPTION_IF_PRESENT("r", zclient, zoptionsarray, zoption)
        SET_CLIENT_OPTION_IF_PRESENT("rw", zclient, zoptionsarray, zoption)
        SET_CLIENT_OPTION_IF_PRESENT("pr", zclient, zoptionsarray, zoption)
    }
    zval_ptr_dtor(&zoptionsarray);

    stripped_path = php_trim(purl->path, strlen(purl->path), "/", 1, NULL, 3 TSRMLS_CC);
    zbucket = create_bucket_object(zclient, stripped_path TSRMLS_CC);
    php_url_free(purl);
    efree(stripped_path);
    if (EG(exception)) {
        zval_ptr_dtor(&zbucket);
        zval_ptr_dtor(&zclient);
        PS_SET_MOD_DATA(NULL);
        return FAILURE;
    } else {
        session_data = ecalloc(1, sizeof(riak_session_data));
        session_data->zbucket = zbucket;
        session_data->zclient = zclient;
        session_data->session_name = estrdup(session_name);
        PS_SET_MOD_DATA(session_data);
        return SUCCESS;
    }
}
/* }}} */

PS_CLOSE_FUNC(riak) /* {{{ */
{
    PS_RIAK_DATA;
    if (data) {
        efree(data->session_name);
        zval_ptr_dtor(&data->zbucket);
        zval_ptr_dtor(&data->zclient);
        efree(data);

        PS_SET_MOD_DATA(NULL);
    }
    return SUCCESS;
}
/* }}} */

PS_READ_FUNC(riak) /* {{{ */
{
    zval *zobject, *zkey, *zdata, *zex;
    PS_RIAK_DATA;
    *vallen = 0;

    MAKE_STD_ZVAL(zkey);
    ZVAL_STRING(zkey, key, 1);

    MAKE_STD_ZVAL(zobject);
    object_init_ex(zobject, riak_object_ce);
    CALL_METHOD1(RiakBucket, getObject, zobject, data->zbucket, zkey);

    if (!EG(exception)) {
        zdata = zend_read_property(riak_object_ce, zobject, "data", sizeof("data")-1, 1 TSRMLS_CC);
        if (zdata->type == IS_STRING && Z_STRLEN_P(zdata) > 0) {
            *vallen = Z_STRLEN_P(zdata);
            *val = emalloc(Z_STRLEN_P(zdata));
            memcpy(*val, Z_STRVAL_P(zdata), Z_STRLEN_P(zdata));
        }
    } else {
        zend_clear_exception(TSRMLS_C);
    }
    if (*vallen == 0) {
        *val = STR_EMPTY_ALLOC();
    }
    zval_ptr_dtor(&zobject);
    zval_ptr_dtor(&zkey);
    return SUCCESS;
}
/* }}} */

PS_WRITE_FUNC(riak) /* {{{ */
{
    zval *zobject;
    PS_RIAK_DATA;
    zobject = create_object_object(key TSRMLS_CC);
    if (EG(exception)) {
        zend_clear_exception(TSRMLS_C);
        zval_ptr_dtor(&zobject);
        return FAILURE;
    }
    zend_update_property_stringl(riak_object_ce, zobject, "data", sizeof("data")-1, val, vallen TSRMLS_CC);
    CALL_METHOD1(RiakBucket, putObject, NULL, data->zbucket, zobject);
    zval_ptr_dtor(&zobject);
    if (EG(exception)) {
        return FAILURE;
    } else {
        return SUCCESS;
    }
}
/* }}} */

PS_DESTROY_FUNC(riak) /* {{{ */
{
    PS_RIAK_DATA;
    zval *zobject;
    zobject = create_object_object(key TSRMLS_CC);
    CALL_METHOD1(RiakBucket, deleteObject, zobject, data->zbucket, zobject);
    zval_ptr_dtor(&zobject);
    if (EG(exception)) {
        return FAILURE;
    }
    return SUCCESS;
}

PS_GC_FUNC(riak) /* {{{ */
{
    /* Do nothing riak´s builtin key expire should be used on the session bucket */
    return SUCCESS;
}
/* }}} */

#endif

