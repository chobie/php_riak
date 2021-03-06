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

#ifndef RIAK_OBJECT__H__
#define RIAK_OBJECT__H__

#include <php.h>
#include <riack.h>

extern zend_class_entry *riak_object_ce;

zval* create_object_object(const char* key TSRMLS_DC);

void riak_object_init(TSRMLS_D);
void set_object_from_riak_content(zval* object, struct RIACK_CONTENT* content TSRMLS_DC);
void set_riak_content_from_object(struct RIACK_CONTENT* content, zval* object, struct RIACK_CLIENT* client TSRMLS_DC);

void riak_key_from_object(zval *zobject, char** key, int* keylen  TSRMLS_DC);

PHP_METHOD(RiakObject, __construct);

#endif
