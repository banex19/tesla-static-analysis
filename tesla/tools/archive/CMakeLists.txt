add_llvm_executable(tesla-archive
  archive.cpp
)

target_link_libraries(tesla-archive
  TeslaCommon
)

target_link_libraries(tesla-archive LLVMSupport)

install(TARGETS tesla-archive DESTINATION bin)
