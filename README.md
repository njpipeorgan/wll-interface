# *wll-interface*

Wolfram [*LibraryLink*](http://reference.wolfram.com/language/LibraryLink/tutorial/Overview.html) is a tool provided by [Wolfram Language](http://www.wolfram.com/language/) (*Mathematica*) to connect to external dynamic libraries. 

*wll-interface* is a header only library written in C++, which makes *LibraryLink* much easier to use. 

## Documentation

See the [Wiki](https://github.com/njpipeorgan/wll-interface/wiki) to get started. 

## Prerequites

To use *wll-interface*, you need: 

* [*Mathematica*](https://www.wolfram.com/mathematica/) 10 or later
* C++ compiler that covers most of the C++17 features
  * [GCC](https://gcc.gnu.org/) 7 or later
  * [Clang](http://clang.llvm.org/) 4 or later
  * MSVC 19.11 ([Visual Studio](https://visualstudio.microsoft.com/vs/) 2017 15.3) or later
  * [ICC](https://software.intel.com/en-us/c-compilers) 19.0 or later

For more information about setting up compilers, see the documentation page for CCompilerDriver [Specific Compilers](https://reference.wolfram.com/language/CCompilerDriver/tutorial/SpecificCompilers.html.en)

## Neat example

In this example, we are going to load a C++ function `multiply` into *Mathmatica* through *LibraryLink*. 

To use *wll-interface* with C++ code, you need to include the header file and use `DEFINE_WLL_FUNCTION` macro to defined the function to be exported:
```Mathematica
src = "
#include \"wll_interface.h\"

double multiply(double x, int y)
{
    return x * y;
}

DEFINE_WLL_FUNCTION(multiply)  // defines wll_multiply
";
```
*wll-interface* will handle the passing of arguments and return value to/from *LibraryLink* through this macro. A new function `wll_multiply` is created here by the library, which is going to be compiled and loaded.

Now we create a shared library from the code. You need to replace `<path-to-wll_interface.h>` below with the directory that contains the header file `wll_interface.h` so that the compiler can find it.
```Mathematica
Needs["CCompilerDriver`"];
mylib = CreateLibrary[src, "wll_multiply", 
  "IncludeDirectories" -> {"<path-to-wll_interface.h>"}, 
  Language -> "C++", "CompileOptions" -> "-std=c++17"]
```

Finally, we load the function into *Mathematica*, and use it:
```Mathematica
multiply = LibraryFunctionLoad[
  mylib, "wll_multiply", {Real, Integer}, Real];
multiply[2.33, 5]
```

**Why does it work?**

*wll-interface* works by effectively creating the following code making calls to *LibraryLink* functions. It is done automatically depending on the type of arguments to `multiply`. 
```C++
EXTERN_C DLLEXPORT int wll_multiply(
    WolframLibraryData, mint argc, MArgument* args, MArgument res)
{
    double arg0 = MArgument_getReal(args[0]);
    int    arg1 = MArgument_getInteger(args[1]);
    double result = multiply(arg0, arg1);
    MArgument_setReal(res, result);
    return LIBRARY_NO_ERROR;
}
```
