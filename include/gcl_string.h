// Copyright 2011 Google Inc. All Rights Reserved.

#include <sstream>
#include <string>

#ifndef GCL_STRING_
#define GCL_STRING_

// Implementation of some of the string utilities from C++0x.
template<typename T>
std::string to_string(const T& t) {
  std::stringstream ss;
  ss << t;
  return ss.str();
}

#endif  //GCL_STRING_
