add_llvm_loadable_module(LLVMTeslaStatic
    InstrumentPass.cpp
)

set_target_properties(LLVMTeslaStatic PROPERTIES
    COMPILE_FLAGS "-fno-rtti"
)

install(TARGETS LLVMTeslaStatic DESTINATION lib)
