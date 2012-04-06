include_directories(.)
include_directories(include)
include_directories(../googletest/include)

add_library(gmock src/gmock-all.cc)
target_link_libraries(gmock gtest)

add_library(gmock_main src/gmock_main.cc)
target_link_libraries(gmock_main gmock)
