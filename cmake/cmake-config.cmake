# This file sets miscellaneous CMake policies that aren't very
# interesting to figuring out what the build system will actually do.
if(COMMAND cmake_policy)
  # Avoids a warning about linking against both absolute paths (like
  # the libraries we build) and searched-for paths (like -lpthread).
  cmake_policy(SET CMP0003 OLD)
  # Avoids a warnings about setting policies in an included file (here).
  cmake_policy(SET CMP0011 OLD)
endif(COMMAND cmake_policy)

# Makes the add_test() command work.
enable_testing()
