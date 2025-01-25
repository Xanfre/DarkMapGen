## About
DarkMapGen is a tool to define and generate auto-map location sub-images from map images used by the Dark engine. More information about its functionality and options can be found in `DarkMapGen-ReadMe.txt`.  
This repository is a fork of the original release of DarkMapGen included with the NewDark binaries. It is currently based on the latest original release.  

## Building
To build the source on Windows, you will need Visual Studio 2008 Professional with Service Pack 1 or MinGW-w64.  

The following additional software is needed.  
[FLTK](https://www.fltk.org/software.php)  
[libpng](http://libpng.org/pub/png/libpng.html)  

Either have these libraries and their dependencies globally available to your compiler and linker or, if using Visual Studio, place them in the `fltk`, `libpng`, and `zlib` subdirectories, respectively. The headers should be in these directories themselves while the libraries should be in the `lib` subdirectory.  

The provided Windows binaries are built With Visual Studio 2008.  
