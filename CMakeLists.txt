
cmake_minimum_required(VERSION 2.8.7)

project(Jet)

find_package(LLVM 3.7.0 REQUIRED CONFIG HINTS C:/users/matthew/desktop/llvm-3.7.0 llvm3.7-appveyor/)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

include_directories("${PROJECT_BINARY_DIR}")

source_group(Types REGULAR_EXPRESSION Types/*)

add_executable(Jet 
	jetc.cpp
	
	Compiler.cpp
	CompilerContext.cpp
	Expressions.cpp
	Parser.cpp
	Parselets.cpp
	Source.cpp
	Types/Types.cpp
	Types/Function.cpp
	Lexer.cpp
	Token.cpp
	Project.cpp
	Compilation.cpp
)

source_group(Types REGULAR_EXPRESSION Types/*)

add_library(Racer
	SHARED
	
	racer.cpp

	Compiler.cpp
	CompilerContext.cpp
	Expressions.cpp
	Parser.cpp
	Parselets.cpp
	Source.cpp
	Types/Types.cpp
	Types/Function.cpp
	Lexer.cpp
	Token.cpp
	Project.cpp
	Compilation.cpp
	)

llvm_map_components_to_libnames(llvm_libs support core mcjit native ipo)

# Link against LLVM libraries
target_link_libraries(Jet ${llvm_libs})

target_link_libraries(Racer ${llvm_libs})