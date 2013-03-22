# Copyright 2011 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

######## Configuration Flags

MAKE_JOBS := $(shell sh util/makejobs.sh)
FLAGS_MAKE = $(MAKE_JOBS) -f ../util/Makefile
FLAGS_98 = CXXFLAGS="-std=c++98 -Wno-c++0x-compat"
FLAGS_11 = CC=gcc CXX=g++ CXXFLAGS=-std=c++0x
FLAGS_DBG = BFLAGS=-g3
FLAGS_OPT = BFLAGS=-O3

######## Primary Rules

default : debug

build : build_opt98 build_opt11

debug : test_dbg98

test : test_dbg98 test_opt98 test_dbg11 test_opt11

######## Build Directories

dbg98 :
	mkdir dbg98

opt98 :
	mkdir opt98

dbg11 :
	mkdir dbg11

opt11 :
	mkdir opt11

clean :
	rm -rf dbg98 opt98 dbg11 opt11

######## C++98

generate_dbg98 : dbg98
	cd dbg98 ; make $(FLAGS_MAKE) generate $(FLAGS_DBG) $(FLAGS_98)
generate_opt98 : opt98
	cd opt98 ; make $(FLAGS_MAKE) generate $(FLAGS_OPT) $(FLAGS_98)

build_dbg98 : generate_dbg98
	cd dbg98 ; make $(FLAGS_MAKE) build $(FLAGS_DBG) $(FLAGS_98)
build_opt98 : generate_opt98
	cd opt98 ; make $(FLAGS_MAKE) build $(FLAGS_OPT) $(FLAGS_98)

test_dbg98 : generate_dbg98
	cd dbg98 ; make $(FLAGS_MAKE) test $(FLAGS_DBG) $(FLAGS_98)
test_opt98 : generate_opt98
	cd opt98 ; make $(FLAGS_MAKE) test $(FLAGS_OPT) $(FLAGS_98)

######## C++11

generate_dbg11 : dbg11
	cd dbg11 ; make $(FLAGS_MAKE) generate $(FLAGS_DBG) $(FLAGS_11)
generate_opt11 : opt11
	cd opt11 ; make $(FLAGS_MAKE) generate $(FLAGS_OPT) $(FLAGS_11)

build_dbg11 : generate_dbg11
	cd dbg11 ; make $(FLAGS_MAKE) build $(FLAGS_DBG) $(FLAGS_11)
build_opt11 : generate_opt11
	cd opt11 ; make $(FLAGS_MAKE) build $(FLAGS_OPT) $(FLAGS_11)

test_dbg11 : generate_dbg11
	cd dbg11 ; make $(FLAGS_MAKE) test $(FLAGS_DBG) $(FLAGS_11)
test_opt11 : generate_opt11
	cd opt11 ; make $(FLAGS_MAKE) test $(FLAGS_OPT) $(FLAGS_11)

