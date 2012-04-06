#ifndef POSIX_ERRORS_
#define POSIX_ERRORS_

#if defined(__GXX_EXPERIMENTAL_CXX0X__)
#include <system_error>
#else
// Minimal subset of <system_error> to get error-throwing to work.
#include <stdexcept>
#include <string.h>
namespace std {
class error_code {
public:
// 19.5.2.2 constructors:
  error_code() : val_(0) {}
  friend inline error_code make_error_code(int e);
  int value() const { return val_; }
private:
  int val_;
};

inline error_code make_error_code(int e) {
  error_code result;
  result.val_ = e;
  return result;
}

class system_error : public runtime_error {
public:
  system_error(error_code ec)
    : runtime_error(strerror(ec.value())), code_(ec) {}
  ~system_error() throw() {}
  const error_code& code() const throw() { return code_; }
  const char* what() const throw() { return what_.c_str(); }
private:
  error_code code_;
  string what_;
};
}  // namespace std
#endif  // defined(__GXX_EXPERIMENTAL_CXX0X__)

inline void handle_err_return(int errc) {
  if (errc == 0) {
    return;
  }
  throw std::system_error(std::make_error_code(errc));
}

#endif  // POSIX_ERRORS_
