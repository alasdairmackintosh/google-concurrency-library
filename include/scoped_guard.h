#ifndef GCL_SCOPED_GUARD_
#define GCL_SCOPED_GUARD_

#include <functional>

namespace gcl {

class scoped_guard {
 public:
  explicit scoped_guard(std::function<void ()> f) : f_(f) {};
  explicit scoped_guard(void f()) : f_(f) {};

  template<typename T>
  explicit scoped_guard(T t) : f_(std::bind(&scoped_guard::call<T>, t)) {};

  scoped_guard(scoped_guard&& other) : f_(std::move(other.f_)) {
    other.dismiss();
  }
  scoped_guard& operator=(scoped_guard&& other) {
    if (this != &other) {
      f_();
      f_ = std::move(other.f_);
      other.dismiss();
    }
    return *this;
  }

  ~scoped_guard() {
    f_();
  }

  void dismiss() {
    f_ = nothing;
  }

 private:
  static void nothing() {};
  template<typename T>
  static void call(T t) { t(); };

  scoped_guard(const scoped_guard&) = delete;;
  scoped_guard& operator=(const scoped_guard& other) = delete;;
  std::function<void ()> f_;
};

}  // namespace gcl


#endif  // GCL_SCOPED_GUARD_
