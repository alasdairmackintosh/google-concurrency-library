#include "thread.h"

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <memory>

namespace {

struct thread_bootstrap_arg {
  std::tr1::function<void()> start_func;
};

static void* thread_bootstrap(void* void_arg) {
  const std::auto_ptr<thread_bootstrap_arg> arg(
    static_cast<thread_bootstrap_arg*>(void_arg));
  // Run the thread.  TODO: This should actually catch and propagate
  // exceptions.
  arg->start_func();
  // C++ threads don't return anything.
  return NULL;
}

}

void thread::start(std::tr1::function<void()> f) {
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  thread_bootstrap_arg* arg = new thread_bootstrap_arg;
  arg->start_func = f;
  joinable_ = true;
  pthread_create(&native_handle_, &attr, thread_bootstrap, arg);
  pthread_attr_destroy(&attr);
}

void thread::join() {
  assert(pthread_join(native_handle_, NULL) == 0);
  joinable_ = false;
}

void thread::detach() {
  assert(pthread_detach(native_handle_) == 0);
  joinable_ = false;
}


void this_thread::sleep_for(const chrono::milliseconds& delay) {
  timespec tm;
  tm.tv_sec = delay.num / 1000;
  tm.tv_nsec = (delay.num % 1000) * 1000 * 1000;
  errno = 0;
  do {
    nanosleep(&tm, &tm);
  } while (errno == EINTR);
}
