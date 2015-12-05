// Copyright 2011 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GCL_DEBUG_
#define GCL_DEBUG_

#include <iostream>

#include <thread>

#include "stream_mutex.h"

namespace gcl {

extern stream_mutex<std::ostream> dbg_stream;

// Prints debug messages to stderr. The output is locked to ensure that only one
// thread can print at a time. Prints out the thread id at the start of the
// line. Sample usage:
//
//  DBG << "Unlocking " << this << std::eol;
//
#define DBG dbg_stream << this_thread::get_id() << " : "

}  // namespace gcl
#endif
