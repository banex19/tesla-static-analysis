set(CMAKE_THREAD_PREFER_PTHREAD)
include(FindThreads)

set(CMAKE_CXX_VISIBILITY_PRESET default)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 0)

add_library(thintesla SHARED
    TeslaAssertionBuilder.cpp
    TeslaInstrument.cpp
    TeslaAllocator.cpp
    TeslaUtils.cpp
    TeslaHashTable.cpp
    MurmurHash3.cpp
)

target_link_libraries(thintesla ${CMAKE_THREAD_LIBS_INIT})


install(TARGETS thintesla DESTINATION lib)
