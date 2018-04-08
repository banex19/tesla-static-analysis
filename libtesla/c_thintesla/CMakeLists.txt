set(CMAKE_THREAD_PREFER_PTHREAD)
include(FindThreads)

set(CMAKE_C_VISIBILITY_PRESET default)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 0)

add_library(cthintesla SHARED
    TeslaAllocator.c
    TeslaUtils.c
    TeslaVector.c
    TeslaMalloc.c
)

target_link_libraries(cthintesla ${CMAKE_THREAD_LIBS_INIT})


install(TARGETS cthintesla DESTINATION lib)
