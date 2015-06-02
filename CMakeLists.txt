
cmake_minimum_required(VERSION 2.8.8)

project(Jet)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

include_directories("${PROJECT_BINARY_DIR}")


add_executable(Jet 
	jetc.cpp
	Compiler.cpp
	Expressions.cpp
	Parser.cpp
	Parselets.cpp
	Source.cpp
	Types.cpp
	Lexer.cpp
)

llvm_map_components_to_libnames(llvm_libs support core mcjit native)

# Link against LLVM libraries
target_link_libraries(Jet ${llvm_libs})