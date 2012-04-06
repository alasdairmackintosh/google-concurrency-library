#ifndef STD_THREAD_
#define STD_THREAD_

#include <pthread.h>
#include <tr1/functional>

// This is a really simple subset of C++0X's std::thread (defined in
// 30.3.1), that we'll use just for testing until we import or write a
// more complete version.
class thread {
public:
  template<class F> explicit thread(F f);

  void join();
  void detach();
private:
  // Deleted
  thread(const thread&);
  void operator=(const thread&);

  void start(std::tr1::function<void()> start_func);

  pthread_t native_handle_;
  bool joinable_;
};

namespace chrono {
struct milliseconds {
  long long num;
  milliseconds(long long num) : num(num) {}
};
}

namespace this_thread {
void sleep_for(const chrono::milliseconds& delay);
}

template<class F>
thread::thread(F f) {
    start(f);
}

#endif  // STD_THREAD_
