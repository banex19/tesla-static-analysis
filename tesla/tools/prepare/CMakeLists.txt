# Dependency tracking: protobuf_generate_cpp() only sort-of works across dirs.
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS tesla.proto)

# Add Clang include directories.
include_directories(
	"${LLVM_SRC}/tools/clang/include"
	"${LLVM_OBJ}/tools/clang/include")

add_llvm_executable(tesla-prepare
	PrepareAST.cpp
	PrepareParser.cpp
	PrepareTool.cpp
	PrepareVisitor.cpp
)


# TeslaCommon no longer gets linked against the full set of LLVM libraries, so
# the analyser needs to be linked against them before it is linked against the
# TESLA library.
install(TARGETS tesla-prepare DESTINATION bin)


#
# Link against Clang and LLVM libraries:
#

# Simply using add_llvm_executable() by itself isn't enough: that doesn't
# link the executable against some important libraries like LLVMSupport.
#
# Instead, we need to pass in ${LLVM_LIBS} explicitly.


# Clang doesn't provide an equivalent to llvm_map_components_to_libraries().
foreach(i
    Tooling Frontend Driver Serialization Parse Sema Analysis 
    Sema Serialization Tooling ASTMatchers AST Edit Lex Basic 
)
  target_link_libraries(tesla-prepare clang${i})
endforeach(i)


target_link_libraries(tesla-prepare LLVMOption)
target_link_libraries(tesla-prepare LLVMSupport)
target_link_libraries(tesla-prepare TeslaCommon)

# There appears to be a bug in one assert in ClangTool. By setting this flag we make sure asserts are disabled. Not ideal.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D NDEBUG")