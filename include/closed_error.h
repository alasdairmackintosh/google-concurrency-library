// Copyright 2010 Google Inc. All Rights Reserved.
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

#ifndef GCL_CLOSED_ERROR_
#define GCL_CLOSED_ERROR_

#include <exception>
#include <stdexcept>
#include <string>

namespace gcl {

// Thrown when attemmpting to read from a closed source, or write to a
// closed sink.
class closed_error : public std::logic_error {
 public:
  explicit closed_error(const std::string& description)
      : std::logic_error(description) {
  }
};

}  // end namespace
#endif  // GCL_CLOSED_ERROR_
