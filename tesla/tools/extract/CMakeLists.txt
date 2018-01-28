add_llvm_executable(tesla-extract
  extract.cpp
)

target_link_libraries(tesla-extract
  TeslaCommon
)

target_link_libraries(tesla-extract LLVMSupport)

install(TARGETS tesla-extract DESTINATION bin)
