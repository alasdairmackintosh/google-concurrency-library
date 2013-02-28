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
FLAGS_0x = CC=gcc CXX=g++ CXXFLAGS=-std=c++0x
FLAGS_DBG = BFLAGS=-g3
FLAGS_OPT = BFLAGS=-O3

######## Primary Rules

default : debug

build : build_opt98 build_opt0x

debug : test_dbg98

test : test_dbg98 test_opt98 test_dbg0x test_opt0x

######## Build Directories

dbg98 :
	mkdir dbg98

opt98 :
	mkdir opt98

dbg0x :
	mkdir dbg0x

opt0x :
	mkdir opt0x

clean :
	rm -rf dbg98 opt98 dbg0x opt0x

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

######## C++0x

generate_dbg0x : dbg0x
	cd dbg0x ; make $(FLAGS_MAKE) generate $(FLAGS_DBG) $(FLAGS_0x)
generate_opt0x : opt0x
	cd opt0x ; make $(FLAGS_MAKE) generate $(FLAGS_OPT) $(FLAGS_0x)

build_dbg0x : generate_dbg0x
	cd dbg0x ; make $(FLAGS_MAKE) build $(FLAGS_DBG) $(FLAGS_0x)
build_opt0x : generate_opt0x
	cd opt0x ; make $(FLAGS_MAKE) build $(FLAGS_OPT) $(FLAGS_0x)

test_dbg0x : generate_dbg0x
	cd dbg0x ; make $(FLAGS_MAKE) test $(FLAGS_DBG) $(FLAGS_0x)
test_opt0x : generate_opt0x
	cd opt0x ; make $(FLAGS_MAKE) test $(FLAGS_OPT) $(FLAGS_0x)

