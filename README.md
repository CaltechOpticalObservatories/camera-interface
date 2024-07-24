# camera-interface
Camera Detector Controller Interface Software

## Requirements:

 - Cmake 3.12 or higher
 - cfitsio and CCFits libraries (expected in /usr/local/lib)


| Archon controllers | ARC controllers |
| -----------------------| ----------------|
| g++ 8.1 or higher (and c++17) | g++ 8.3 (and c++17) |
|                               | ARC API 3.6 and Arc66PCIe driver |


## Build instructions:

- change to the build directory

- To start with a clean build, delete the contents of the build
  directory, including the subdirectory CMakeFiles/,
  but not the `.gitignore file`. For example:

```
   % cd build
   % rm -Rf *
```

- create the Makefile by running cmake (from the build directory),

| Archon                       | ARC                                                    |
|------------------------------|--------------------------------------------------------|
| `$ cmake -DINSTR=generic ..` | `$ cmake -DINSTR=generic -DINTERFACE_TYPE=AstroCam ..` |


- compile the sources,

```
   $ make
```

 - run the program using one of these forms, 

```
   $ ../bin/camerad <file.cfg>
   $ ../bin/camerad -f <file.cfg>
   $ ../bin/camerad -d -f <file.cfg>
```   

   where <file.cfg> is an appropriate configuration file. See the example .cfg files
   in the Config directory of this distribution. Note that the -f option specifies 
   a config file and the -d option forces it to run as a daemon, overriding any 
   DAEMON=no setting in the configuration file.

 - Note that only when INTERFACE_TYPE is set to Archon will the emulator software
 be compiled.

---

David Hale <dhale@astro.caltech.edu>

