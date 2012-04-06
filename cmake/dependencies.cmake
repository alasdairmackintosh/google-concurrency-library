# This file searches for and declares our dependencies.
include(CheckLibraryExists)
check_library_exists(pthread pthread_create "" HAVE_LIBPTHREAD)
set(system_libs ${system_libs} pthread)
