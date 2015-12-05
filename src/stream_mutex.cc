// Copyright 2012 Google Inc. All Rights Reserved.
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

#include <iostream>

#include "stream_mutex.h"

std::recursive_mutex* get_stream_mutex_map( void *stm_ptr )
{
    static std::mutex map_mutex;
    static std::unordered_map<void*, std::recursive_mutex*> stream_map( 19 );
    std::lock_guard<std::mutex> _(map_mutex);
    std::recursive_mutex*& mtx_ptr = stream_map[stm_ptr];
    if (!mtx_ptr)
        mtx_ptr = new std::recursive_mutex;
    return mtx_ptr;
}

stream_mutex<std::istream> mcin(std::cin);
stream_mutex<std::ostream> mcout(std::cout);
stream_mutex<std::ostream> mcerr(std::cerr);
stream_mutex<std::ostream> mclog(std::clog);

stream_mutex<std::wistream> mwcin(std::wcin);
stream_mutex<std::wostream> mwcout(std::wcout);
stream_mutex<std::wostream> mwcerr(std::wcerr);
stream_mutex<std::wostream> mwclog(std::wclog);
