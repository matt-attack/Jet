
cmake_minimum_required(VERSION 2.8.7)
project(jet)

find_package(LLVM 7.0.0 REQUIRED CONFIG HINTS C:/users/space/desktop/llvm/llvm-7.0.0.src)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

include_directories("${PROJECT_BINARY_DIR}" src)

file(GLOB_RECURSE EXPRESSION_SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "src/expressions/*.cpp")

add_executable(jet 
	src/tools/jetc.cpp

    ${EXPRESSION_SOURCES}
	
	src/Compiler.cpp
	src/CompilerContext.cpp
    src/Diagnostics.cpp
	src/Parser.cpp
	src/Parselets.cpp
	src/Source.cpp
	src/types/Types.cpp
	src/types/Function.cpp
    src/types/Namespace.cpp
    src/types/Struct.cpp
	src/Lexer.cpp
	src/Token.cpp
	src/Project.cpp
	src/Compilation.cpp
)
target_compile_features(jet PRIVATE cxx_range_for)

add_executable(test_runner 
	src/tools/test_runner.cpp
)
target_compile_features(test_runner PRIVATE cxx_range_for)

#source_group(Types REGULAR_EXPRESSION Types/*)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g ")
elseif(COMPILER_SUPPORTS_CXX0X)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -g ")
else()
    message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

llvm_map_components_to_libnames(llvm_libs support core mcjit native ipo ARM)

# Link against LLVM libraries
target_link_libraries(jet ${llvm_libs})# "C:/Users/Matthew/Desktop/cfe-3.7.1.src/Debug/lib/libclang.lib")

