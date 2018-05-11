add_llvm_loadable_module(LLVMTeslaStatic
    StaticOptimizer.cpp
    "./../ThinTeslaAssertion.cpp"
    "./../Remove.cpp"
)

#set_target_properties(LLVMTeslaStatic PROPERTIES
#    COMPILE_FLAGS "-fno-rtti"
#

target_link_libraries(LLVMTeslaStatic
  TeslaCommon
)


include_directories("./../")
include_directories("${CMAKE_SOURCE_DIR}/../include")

install(TARGETS LLVMTeslaStatic DESTINATION lib)
