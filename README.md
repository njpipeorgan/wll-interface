# *wll-interface*
Wolfram *LibraryLink* provides users a way to call external programs in Wolfram Language (*Mathematica*). The external binary codes are loaded into the Wolfram Kernel through dynamic libraries ([.dll](https://en.wikipedia.org/wiki/Dynamic-link_library "Dynamic-link library - Wikipedia"), [.so](https://en.wikipedia.org/wiki/Library_(computing)#Shared_libraries "Library (computing) - Wikipedia")). Therefore, compared to WSTP, the overhead of calling a function using *LibraryLink* is much lower, and argument passing is much more efficient. [Wolfram LibraryLink User Guide](http://reference.wolfram.com/language/LibraryLink/tutorial/Overview.html) gives a comprehend description of *LibraryLink*.

Although *LibraryLink* makes it possible to improve runtime efficiency, it comes at the cost of writing tedious boilerplate code, in addition to writing functions in C/C++. *wll-interface* is a header only library written in C++, which simplifies *LibraryLink* coding and make it less error-prone. 

## In a nutshell

In this section, we are going to write a simple math function in C++, compile it into a dynamic-link library using MSVC v19.14 (ship with [Visual Studio 2017](https://visualstudio.microsoft.com/vs/) v15.7), and load it from *Mathematica*.

#### Create a project

#### Write the function

#### Load the library


## How does *wll-interface* work?
