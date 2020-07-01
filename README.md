# camera-interface
Camera Detector Controller Interface Software

Build instructions.

 - Cmake 3.5 and g++ are required.

 - cfitsio and CCFits are required

 - ARC API 2.1 and 3.5 are required

 - change to the build directory

 - To start with a clean build, delete the contents of the build
   directory, including the subdirectory CMakeFiles/, 
   but not the .gitignore file:

   % cd build
   % rm -Rf *

 - create the Makefile by running cmake,

   % cmake ..

 - compile the sources,

   % make

 - run the program,

   % ../bin/archonserver

---

David Hale <dhale@astro.caltech.edu>

