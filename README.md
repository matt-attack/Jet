
 Jet
==========

A work in progress compiled language that uses a LLVM backend. Currently the language is much like C, but many things will change as time goes on.

Currently the build only works on Windows, but it shouldn't take much more than changing a few function calls to port to Linux/Mac.

In order to build the compiler you need to have LLVM and in order to use the compiler you need GCC or the MSVC linker (link.exe) installed and in your PATH.

#### Notable Features So Far:
- Structs
	- Constructors/Destructors
	- Member Functions
	- Extension Methods
	- Templates
- Type Inference
- Traits That Automatically Apply Themselves to All Applicable Types


# Using Jet and The Compiler

This is a short introductory tutorial to Jet and its compiler.

## Projects
To create your first Jet program, you first need to create a new folder somewhere on your system with a file named "project.jp".

An example project.jp file should look as follows:
```cpp
[lib:] (only if you are making a library, not an executable)
files: (space delimited list of all source files in your program)
requires: (space delimited list of paths to all required libraries)
[libs:] (space delimited list of libraries to link to)
```
Each section of the project file can span any number of lines.


## Code
Lets start by making a source file named "main.jet" and adding it to the list of files in the project file.

Inside this file put:
```cpp
extern fun int puts(char* text);

fun int main()
{
	puts("Hello World!");

	return 0;
}
```


## Building
In order to run your program, navigate your terminal to the directory in which your program resides.
Then run the command "jet -build".

Now, there will be an executable named "program.exe" in your project directory.
This program, when executed, will output Hello World! to console.
