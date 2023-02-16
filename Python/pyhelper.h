#ifndef PYHELPER
#define PYHELPER

#include <Python.h>
#include <iostream>

class CPyInstance {
  public:
    char *restorePythonPath;
    CPyInstance() {
      this->restorePythonPath = std::getenv( "PYTHONPATH" );
      if ( setenv( "PYTHONPATH", "/usr/local/caltech/nirc2/Python", 1 ) != 0 ) {
        std::cerr << "(CPyInstance::CPyInstance) ERROR setting PYTHONPATH: " << errno << "\n";
      }
      Py_Initialize();
    }

    ~CPyInstance() {
      Py_Finalize();
//    if ( setenv( "PYTHONPATH", this->restorePythonPath, 1 ) != 0 ) {
//      std::cerr << "(CPyInstance::~CPyInstance) ERROR restoring PYTHONPATH: " << errno << "\n";
//    }
    }
};

class CPyObject {
  private:
    PyObject *p;
  public:
    CPyObject() : p(NULL) {
    }

    CPyObject(PyObject* _p) : p(_p) {
    }

    ~CPyObject() {
      Release();
    }

    PyObject* getObject() {
      return p;
    }

    PyObject* setObject(PyObject* _p) {
      return (p=_p);
    }

    PyObject* AddRef() {
      if(p) {
        Py_INCREF(p);
      }
      return p;
    }

    void Release() {
      if(p) {
        Py_DECREF(p);
      }

      p= NULL;
    }

    PyObject* operator ->() {
      return p;
    }

    bool is() {
      return p ? true : false;
    }

    operator PyObject*() {
      return p;
    }

    PyObject* operator = (PyObject* pp) {
      p = pp;
      return p;
    }

    operator bool() {
      return p ? true : false;
    }
};


#endif
