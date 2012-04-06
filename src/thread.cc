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

thread::id thread::get_id() const {
  if (joinable_) {
    return thread::id(native_handle_);
  } else {
    return thread::id();
  }
}

thread::id::id() : is_null_(true) {
}

thread::id::id(pthread_t id) : id_(id), is_null_(false) {
}

thread::id this_thread::get_id() {
  return thread::id(pthread_self());
}

bool operator==(thread::id x, thread::id y) {
  return (x.is_null_ && y.is_null_) ||
      (!x.is_null_ && !y.is_null_ && pthread_equal(x.id_, y.id_));
}

bool operator!=(thread::id x, thread::id y) {
  return !(x == y);
}

// TODO(alasdair): Used for comparison operator. Need better
// implementation of this, or alternative way to implement these
// methods.
unsigned long thread::id::long_value() const {
  assert(!is_null_ && sizeof(id_) == sizeof(unsigned long));
  unsigned long tid;
  memcpy(&tid, &id_, sizeof(unsigned long));
  return tid;
}

// Comparison. To satisfy an equivalence relationship induced by a
// strict weak ordering, we define a and b to be equivalent if
// !(a < b) && !(b < a). See 25.5.7. To ensure that two null ids
// are equivalent, we ensure that nothing is ever less than null.
bool operator<(thread::id x, thread::id y) {
  if (y.is_null_) {
    return false;
  } else if (x.is_null_) {
    return true;
  } else {
    return x.long_value() <  y.long_value();
  }
}

bool operator<=(thread::id x, thread::id y) {
  return ! (y < x);
}

bool operator>(thread::id x, thread::id y) {
  return ! (x <= y);
}

bool operator>=(thread::id x, thread::id y) {
  return ! (x < y);
}
