# *wll-interface*
Wolfram *LibraryLink* provides users a way to call external programs in Wolfram Language (*Mathematica*). The external binary codes are loaded into the Wolfram Kernel through dynamic libraries ([.dll](https://en.wikipedia.org/wiki/Dynamic-link_library "Dynamic-link library - Wikipedia"), [.so](https://en.wikipedia.org/wiki/Library_(computing)#Shared_libraries "Library (computing) - Wikipedia")). Therefore, compared to WSTP, the overhead of calling a function using *LibraryLink* is much lower, and argument passing is much more efficient. [Wolfram LibraryLink User Guide](http://reference.wolfram.com/language/LibraryLink/tutorial/Overview.html) gives a comprehend description of *LibraryLink*.

Although *LibraryLink* makes it possible to improve runtime efficiency, it comes at the cost of writing tedious boilerplate code, in addition to writing functions in C/C++. *wll-interface* is a header only library written in C++, which simplifies *LibraryLink* coding and make it less error-prone. 

## Walk-through

In this section, we are going to write a simple math function in C++, compile it into a dynamic-link library using MSVC v19.14 (ship with [Visual Studio 2017](https://visualstudio.microsoft.com/vs/) v15.7), and load it from *Mathematica*.

### Create a project

First, we create a empty project.

1. Choose **File > New > Project** to open the **New Project** dialog box.
2. Select **Installed > Visual C++ > General**, and select **Empty Project** in the center pane. In the **Name** box, enter the project name *MyLibraryLinkProject*. Then click **OK** button.
4. Choose **Project > Add New Item** on the menu bar to open the dialog box.
5. Select **Installed > Visual C++**, and and select **C++ File (.cpp)** in the center pane. In the **Name** box, enter *functions.cpp*. Then click **OK** button.

Second, we manage the properties of our project.

1. Choose **Project > MyLibraryLinkProject Properties...** to open the **Property Pages** dialog box.  
2. On the top of the dialog box, choose **All Configuration** in the **Configuration** drop-down list, and **x64** in the **Platform** drop-down list.
3. Select **Configuration Properties > C/C++ > Language**. In the right pane, find **C++ Language Standard** Property, and choose **ISO C++17 Standard (/std:c++17)**.

Third, we include *wll_interface.h* header file into our project.

1. Download *wll_interface.h*, and save it to the *Mathematica* C/C++ IncludeFiles directory [Where is this directory?](#faq).
2. Choose **Project > MyLibraryLinkProject Properties...** to open the **Property Pages** dialog box.  
3. Select **Configuration Properties > C/C++ > General**. In the right pane, choose the drop-down arrow to the right of **Additional Include Directories** property, then select **<Edit...>** to open the dialog.
4. Click **New Line** button on the top, and click **...** button to open the **Select Directory** dialog box. 
5. Find the *Mathematica* C/C++ IncludeFiles directory, and click **Select Folder** button. 
6. ...

### Write the function

### Load the library


## How does *wll-interface* work?


## FAQ

**Where is *Mathematica* C/C++ IncludeFiles directory?**  
Evaluate the following expression in *Mathematica*: 

    SystemOpen[$InstallationDirectory<>"\\SystemFiles\\IncludeFiles\\C"]



