
cmake_minimum_required(VERSION 2.8.7)

project(Jet)


find_package(LLVM 3.7.0 REQUIRED CONFIG HINTS C:/users/matthew/desktop/llvm-3.7.0 llvm3.7-appveyor/llvm-3.7)
find_package(Clang REQUIRED CONFIG HINTS C:/Users/Matthew/Desktop/cfe-3.7.1.src)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

message(STATUS "Found CLANG ${CLANG_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${Clang_DIR}")

set(CLANG_PATH "${Clang_DIR}/../../../")
set(CLANG_INCLUDE_DIR "C:/Users/Matthew/Desktop/cfe-3.7.1.src/include") #"$(CLANG_PATH}/include")

include_directories(${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIR})
add_definitions(${LLVM_DEFINITIONS})

get_cmake_property(_variableNames VARIABLES)
foreach (_variableName ${_variableNames})
    message(STATUS "${_variableName}=${${_variableName}}")
endforeach()

include_directories("${PROJECT_BINARY_DIR}" )#  "C:/Users/Matthew/Desktop/cfe-3.7.1.src/include")

source_group(Types REGULAR_EXPRESSION Types/*)

add_executable(Jet 
	jetc.cpp
	
	Compiler.cpp
	CompilerContext.cpp
	Expressions.cpp
	DeclarationExpressions.cpp
	ControlExpressions.cpp
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
	ControlExpressions.cpp
	DeclarationExpressions.cpp
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

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g")
elseif(COMPILER_SUPPORTS_CXX0X)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -g")
else()
        message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

# target_compile_features(Racer PRIVATE cxx_range_for)
# target_compile_features(Jet PRIVATE cxx_range_for)

llvm_map_components_to_libnames(llvm_libs support core mcjit native ipo)

# Link against LLVM libraries
target_link_libraries(Jet ${llvm_libs} "C:/Users/Matthew/Desktop/cfe-3.7.1.src/Debug/lib/libclang.lib")

target_link_libraries(Racer ${llvm_libs})