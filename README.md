# archon-interface
STA Archon Detector Controller Interface Software

Build instructions.

 - Cmake 2.8 and g++ are required.

 - change to the build directory

 - To start with a really clean build, delete the contents of the build
   directory, including the subdirectory CMakeFiles/, 
   but not the .gitignore file:

   % cd build
   % rm -Rf *

 - create the Makefile by running cmake,

   % cmake ..

 - compile the sources,

   % make

 - run the program,

   % ../bin/archon-interface

---

David Hale <dhale@astro.caltech.edu>

