cmake_minimum_required(VERSION 2.6)
project(libtesla)

# More warnings (used in the FreeBSD "buildworld" step).
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wmissing-prototypes -Wpointer-arith")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wreturn-type -Wcast-qual")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wwrite-strings -Wswitch -Wshadow")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wunused-parameter -Wcast-align")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wchar-subscripts -Winline")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wnested-externs -Wredundant-decls")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wold-style-definition")

# For some reason, shared libs on zenith aren't always build with -fPIC...
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_DEBUG")
endif (CMAKE_BUILD_TYPE STREQUAL "Debug")

if (CMAKE_BUILD_TYPE STREQUAL "Release")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DRELEASE")
endif (CMAKE_BUILD_TYPE STREQUAL "Release")



add_subdirectory(src)
add_subdirectory(thintesla)
add_subdirectory(c_thintesla)
add_subdirectory(test)

