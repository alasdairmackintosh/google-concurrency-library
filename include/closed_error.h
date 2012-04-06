// Copyright 2010 Google Inc. All Rights Reserved.

#ifndef GCL_CLOSED_ERROR_
#define GCL_CLOSED_ERROR_

#include <exception>
#include <stdexcept>
#include <string>

using std::logic_error;
using std::string;

namespace gcl {

// Thrown when attemmpting to read from a closed source, or write to a
// closed sink.
class closed_error : public logic_error {
 public:
  explicit closed_error(const string& description)
      : logic_error(description) {
  }
};

}  // end namespace
#endif  // GCL_CLOSED_ERROR_
