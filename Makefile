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
FLAGS_11 = CC=gcc CXX=g++ CXXFLAGS=-std=c++11
FLAGS_DBG = BFLAGS=-g3
FLAGS_OPT = BFLAGS=-O3

######## Primary Rules

default : debug

build : build_opt11

debug : test_dbg11

test : test_dbg11 test_opt11

install : test_opt11
	cd opt11 ; make $(FLAGS_MAKE) install

######## Build Directories

dbg11 :
	mkdir dbg11

opt11 :
	mkdir opt11

clean :
	rm -rf dbg11 opt11


######## C++11

build_dbg11 : dbg11
	cd dbg11 ; make $(FLAGS_MAKE) build $(FLAGS_DBG) $(FLAGS_11)
build_opt11 : opt11
	cd opt11 ; make $(FLAGS_MAKE) build $(FLAGS_OPT) $(FLAGS_11)

test_dbg11 : dbg11
	cd dbg11 ; make $(FLAGS_MAKE) test $(FLAGS_DBG) $(FLAGS_11)
test_opt11 : opt11
	cd opt11 ; make $(FLAGS_MAKE) test $(FLAGS_OPT) $(FLAGS_11)

