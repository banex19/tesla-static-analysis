add_llvm_executable(tesla-print print.cpp)
target_link_libraries(tesla-print TeslaCommon)
target_link_libraries(tesla-print LLVMSupport)
install(TARGETS tesla-print DESTINATION bin)
