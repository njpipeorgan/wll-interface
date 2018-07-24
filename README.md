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
  * MSVC 19.11 ([VS](https://visualstudio.microsoft.com/vs/) 2017 15.3) or later
  * [ICC](https://software.intel.com/en-us/c-compilers) 19.0 or later

## Neat example

After including `wll_interface.h` and adding *Mathematica* C/C++ IncludeFiles Directory (`$InstallationDirectory/SystemFiles/IncludeFiles/C`) to your compiler's include directories, you are ready to use *wll-interface*. 

Compile the following code into a dynamic library: 

    #include "wll_interface.h"
    double multiply(double x, int y)
    {
        return x * y;
    }
    DEFINE_WLL_FUNCTION(multiply)  // defines "wll_multiply"

Load the *LibraryLink* function into *Mathematica*, and use the function:

    multiply = LibraryFunctionLoad[
                 (*full path to the dynamic library*), "wll_multiply", {Real, Integer}, Real];
    multiply[2.33, 5]

The equivalent code that directly calls *LibraryLink* APIs is shown as follows: 

    EXTERN_C DLLEXPORT int wll_multiply(
        WolframLibraryData, mint argc, MArgument* args, MArgument res)
    {
        double b = MArgument_getReal(args[0]);
        int    p = MArgument_getInteger(args[1]);
        double result = b * p;
        MArgument_setReal(res, result);
        return LIBRARY_NO_ERROR;
    }
