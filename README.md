# camera-interface
Camera Detector Controller Interface Software

## Requirements:

 - Cmake 3.12 or higher
 - cfitsio and CCFits libraries (expected in /usr/local/lib)
 
### For ARC controllers,
  - g++ 8.3 (and c++17)
  - ARC API 3.6 and Arc66PCIe driver
 
### For Archon
 - g++ 4.8 or higher (and c++11)

## Build instructions:

 - edit the CMakeLists.txt file to un-comment ONE of the following two lines
 according to the controller interface you are using:

```
set(INTERFACE_TYPE "Archon")
#set(INTERFACE_TYPE "AstroCam")
```

 - change to the build directory

 - To start with a clean build, delete the contents of the build
   directory, including the subdirectory CMakeFiles/, 
   but not the `.gitignore file`. For example:
   
```
   % cd build
   % rm -Rf *
```

 - create the Makefile by running cmake (from the build directory),

```
   % cmake ..
```

- compile the sources,

```
   % make
```

 - run the program,

```
   % ../bin/archonserver <file.cfg>
```   

   where <file.cfg> is an appropriate configuration file. See the example .cfg files
   in this distribution.

---

David Hale <dhale@astro.caltech.edu>

