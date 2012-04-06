# See http://cmake.org/cmake/help/cmake2.4docs.html for documentation.
cmake_minimum_required(VERSION 2.4)
project(GoogleConcurrencyLibrary)

# Add srcdir/cmake to the search path for include() statements.
set(CMAKE_MODULE_PATH
  ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)

include(cmake-config)
include(dependencies)

add_subdirectory(third_party/googletest)
add_subdirectory(third_party/googlemock)

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/third_party/googletest/include)
include_directories(${PROJECT_SOURCE_DIR}/third_party/googlemock/include)

# Main libraries
add_library(std_thread src/thread.cc src/mutex.cc src/condition_variable.cc)
target_link_libraries(std_thread ${system_libs})


# Unit Tests  (TODO: Define helpful macros)
add_executable(thread_test testing/thread_test.cc)
target_link_libraries(thread_test gmock_main std_thread)
add_test(thread_test thread_test)

add_executable(lock_test testing/lock_test.cc)
target_link_libraries(lock_test gmock_main std_thread)
add_test(lock_test lock_test)

add_executable(race_test testing/race_test.cc)
target_link_libraries(race_test gmock_main std_thread)
add_test(race_test race_test)
