#summary Testing "Google Concurrency Library for C++" with data race detectors



# Introduction #
We want to simplify race detection for the programs that use "Google Concurrency Library for C++" and to make sure the library itself has no races.

# Tools #

Currently supported tools:
  * [ThreadSanitizer](http://code.google.com/p/data-race-test/wiki/ThreadSanitizer)

Tools we want to support:
  * [Helgrind](http://valgrind.org/docs/manual/hg-manual.html)
  * [DRD](http://valgrind.org/docs/manual/drd-manual.html)
  * [Intel Parallel Inspector](http://software.intel.com/en-us/intel-parallel-inspector)
  * More?

# ThreadSanitizer #
Google Concurrency Library comes with pre-compiled binaries of
[ThreadSanitizer](http://code.google.com/p/data-race-test/wiki/ThreadSanitizer)
for the following platforms:
  * Linux/x86\_64
  * MacOS X 10.5

## Example ##
You can run ThreadSanitizer on `race_test` or any other test:
```
  srcdir=<the directory with GCL sources>
  objdir=<the directory where GCL is built>
  # Linux: 
  TSAN="$srcdir/third_party/ThreadSanitizer/Linux-x86_64/tsan-self-contained.sh"
  # Mac:   
  TSAN="$srcdir/third_party/ThreadSanitizer/Darwin-i386/tsan-self-contained.sh --dsymutil=yes"
  cd $objdir
  # Run in the pure-happens-before (less aggressive) mode.
  $TSAN --pure-happens-before ./race_test  
  # Run in the hybrid (more aggressive) mode.
  $TSAN ./race_test
```

The two major modes of ThreadSanitizer are:
  * Hybrid (default): finds more races, but may report false positives.
  * Pure Happens-before (add `--pure-happens-before` flag): finds less races; less predictable; will report false positives **only** if the program has custom lock-free synchronization.


Example of output (from `testing/race_test.cc`, `ThreadTest.SimpleDataRaceTest`):
```
==10430== WARNING: Possible data race during read of size 4 at 0x7FF000144: {{{
==10430==    T2 (locks held: {}):
==10430==     #0  IncrementArg1(int*) testing/race_test.cc:16
==10430==     #1  std::tr1::_Bind<void (*()(int*))(int*)>::operator()() /usr/include/c++/4.2/tr1/bind_iterate.h:45
==10430==     #2  std::tr1::_Function_handler<...>::_M_invoke(std::tr1::_Any_data const&) /usr/include/c++/4.2/tr1/functional_iterate.h:502
==10430==     #3  std::tr1::function<void ()()>::operator()() const /usr/include/c++/4.2/tr1/functional_iterate.h:865
==10430==     #4  (anonymous namespace)::thread_bootstrap(void*) src/thread.cc:20
==10430==     #5  ThreadSanitizerStartThread /tmp/tsan/valgrind/tsan/ts_valgrind_intercepts.c:504
==10430==   Concurrent write(s) happened at (OR AFTER) these points:
==10430==    T1 (locks held: {}):
==10430==     #0  IncrementArg1(int*) testing/race_test.cc:15
==10430==     #1  std::tr1::_Bind<void (*()(int*))(int*)>::operator()() /usr/include/c++/4.2/tr1/bind_iterate.h:45
==10430==     #2  std::tr1::_Function_handler<...>::_M_invoke(std::tr1::_Any_data const&) /usr/include/c++/4.2/tr1/functional_iterate.h:502
==10430==     #3  std::tr1::function<void ()()>::operator()() const /usr/include/c++/4.2/tr1/functional_iterate.h:865
==10430==     #4  (anonymous namespace)::thread_bootstrap(void*) src/thread.cc:20
==10430==     #5  ThreadSanitizerStartThread /tmp/tsan/valgrind/tsan/ts_valgrind_intercepts.c:504
==10430==   Location 0x7FF000144 is 3764 bytes inside T0's stack [0x7FE800FF8,0x7FF000FF8]
==10430== }}}
```


For more details and examples refer to [ThreadSanitizer](http://code.google.com/p/data-race-test/wiki/ThreadSanitizer) home page.

# Helgrind, DRD #
You may try **Helgrind** or **DRD** which come as a part of [Valgrind](http://www.valgrind.org) distribution.

Example:
```
valgrind --tool=helgrind ./race_test 
valgrind --tool=drd ./race_test 
```

Helgrind and DRD from Valgrind 3.5 are somewhat similar
to ThreadSanitizer in pure happens-before mode, but have different amount of false negatives/positives and different details in output.

# Dynamic Annotations #
[Dynamic Annotations](http://code.google.com/p/data-race-test/wiki/DynamicAnnotations)
allow to explain custom synchronization and expected/benign race to race detectors.

> TODO: add support of Dynamic Annotations to "Google Concurrency Library for C++"