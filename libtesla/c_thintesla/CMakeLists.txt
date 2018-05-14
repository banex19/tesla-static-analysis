set(CMAKE_THREAD_PREFER_PTHREAD)
include(FindThreads)

set(CMAKE_C_VISIBILITY_PRESET default)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 0)

add_library(cthintesla SHARED
    TeslaAllocator.c
    TeslaUtils.c
    TeslaVector.c
    TeslaMalloc.c
    TeslaMallocStatic.c
    TeslaHashTable.c
    TeslaLogic.c
    TeslaLogicPerThread.c
    TeslaLogicLinearHistory.c
    TeslaHistory.c
    TeslaState.c
    TeslaStore.c
    TeslaHash.c
    TeslaAssert.c
    MurmurHash3.c
)

# Enable LTO.
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -flto")

# For 'perf'.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ggdb")

target_link_libraries(cthintesla ${CMAKE_THREAD_LIBS_INIT})


install(TARGETS cthintesla DESTINATION lib)
