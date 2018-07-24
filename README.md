# *wll-interface*
Wolfram *LibraryLink* provides users a way to call external programs in Wolfram Language (*Mathematica*). The external binary codes are loaded into the Wolfram Kernel through dynamic libraries ([.dll](https://en.wikipedia.org/wiki/Dynamic-link_library "Dynamic-link library - Wikipedia"), [.so](https://en.wikipedia.org/wiki/Library_(computing)#Shared_libraries "Library (computing) - Wikipedia")). Therefore, compared to WSTP, the overhead of calling a function using *LibraryLink* is much lower, and argument passing is much more efficient. [Wolfram LibraryLink User Guide](http://reference.wolfram.com/language/LibraryLink/tutorial/Overview.html) gives a comprehend description of *LibraryLink*.

Although *LibraryLink* makes it possible to improve runtime efficiency, it comes at the cost of writing tedious boilerplate code, in addition to writing functions in C/C++. *wll-interface* is a header only library written in C++, which simplifies *LibraryLink* coding and make it less error-prone. 

## Walk-through

In this section, we are going to write a simple math function in C++, compile it into a dynamic-link library using MSVC v19.14 (ship with [Visual Studio 2017](https://visualstudio.microsoft.com/vs/) v15.7), and load it from *Mathematica*.

### Create a project

First, we create a empty project in VS2017.

1. Choose **File > New > Project** to open the **New Project** dialog box.
2. Select **Installed > Visual C++ > General**, and select **Empty Project** in the center pane. In the **Name** box, enter the project name *MyLibraryLinkProject*. Then click **OK** button.
4. Choose **Project > Add New Item** on the menu bar to open the dialog box.
5. Select **Installed > Visual C++**, and and select **C++ File (.cpp)** in the center pane. In the **Name** box, enter *functions.cpp*. Then click **OK** button. We will write our *LibraryLink* function in this file later.

Second, we manage the properties of our project and include *wll-interface* into the project.

1. Download *wll_interface.h*, and save it to the *Mathematica* C/C++ IncludeFiles directory ([Where is this directory?](#faq)).
2. Choose **Project > MyLibraryLinkProject Properties...** to open the **Property Pages** dialog box. 
3. On the top of the dialog box, choose **All Configuration** in the **Configuration** drop-down list, and **x64** in the **Platform** drop-down list. 
4. Click **Configuration Manager...** button, and choose **Debug** for **Active Solution Configuration**, and **x64** for **Active Solution Platform**. Then click **Close** button. 
5. Select **Configuration Properties > General**. Find **Project Defaults > Configuration Type**, and choose **Dynamic Library (.dll)**. 
6. Select **Configuration Properties > C/C++ > Command Line**. In the **Additional Options** edit box, enter:  
`/std:c++17 /I "[your Mathematica C/C++ IncludeFiles directory]"`  
For example, if you install *Mathematica* under `C:\Program Files\Wolfram Research\Mathematica\11.3`, you might enter:  
`/std:c++17 /I "C:\Program Files\Wolfram Research\Mathematica\11.3\SystemFiles\IncludeFiles\C"` 
7. Click **OK** button to save all changes.  

### Write and compile the function

In the **Solution Explorer** window, click **Source Files > function.cpp** to open it. Enter the following code. 

    #include "wll_interface.h"           // include wll interface library
    
    double power(double b, int p)        // defines the function "power"
    {
        double result = std::pow(b, p);  // calculate b^p
        return result;                   // return the result
    }
    
    DEFINE_WLL_FUNCTION(power)           // defines the LibraryLink function "wll_power"

Note that `DEFINE_WLL_FUNCTION` is a macro defined in `wll_interface.h`, which expands to a *LibraryLink* function that handles argument passing, including matching the argument types between Wolfram Language and C++. This macro will generate the *LibraryLink* function by prepending `wll_` to the C++ function to avoid name collision. 

Choose **Build > Build Solution** on the menu bar. If the compilation is successful, the path to the output library is shown in the **Output** window, as `...\...\MyLibraryLinkProject.dll`.

### Load the library

Open *Mathematica*, then set `mylib` to the full path of the .dll file in the previous step:

    mylib = "...\\...\\MyLibraryLinkProject.dll";

Load the *LibraryLink* function `wll_power` from `mylib`, and provide the argument types (Real, Integer) and the result type (Real). 

    power = LibraryFunctionLoad[mylib, "wll_power", {Real, Integer}, Real];

The function can be called just like a normal Wolfram Language function: 

    power[3.14, 2]     (* gives 9.8596 *)

If you want to make any change to the source code and recompile it, unload the library from *Mathematica* so that the compiler can overwrite the .dll file.

    LibraryUnload[mylib];


## How does *wll-interface* work?


## FAQ

**Where is *Mathematica* C/C++ IncludeFiles directory?**  
Evaluate the following expression in *Mathematica*:   
`SystemOpen[$InstallationDirectory<>"\\SystemFiles\\IncludeFiles\\C"]`



