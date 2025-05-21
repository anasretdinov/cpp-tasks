if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

set(CMAKE_CXX_FLAGS_ASAN "${CMAKE_CXX_FLAGS} -g3 -fsanitize=address,leak,undefined -fno-sanitize-recover=all"
  CACHE STRING "Compiler flags in asan build"
  FORCE)
