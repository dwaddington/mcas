
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#define PY_ARRAY_UNIQUE_SYMBOL mcas_ARRAY_API
#include <numpy/arrayobject.h>

//now, everything is setup, just include the numpy-arrays:
#include <numpy/arrayobject.h>
#include <Python.h>
#include <structmember.h>
#include <objimpl.h>
#include <pythread.h>
//#include <numpy/npy_math.h>

#include <api/components.h>
#include <api/kvstore_itf.h>

#if defined(__linux__)
#include <execinfo.h>
#endif

#include <list>
#include <common/logging.h>


// forward declaration of custom types
//
extern PyTypeObject ZcStringType;
extern PyTypeObject SessionType;
extern PyTypeObject PoolType;

static PyMethodDef mcas_methods[] = {
  {NULL, NULL, 0, NULL}        /* Sentinel */
};

static PyModuleDef mcas_module = {
    PyModuleDef_HEAD_INIT,
    "mcas",
    "mcas client API extension module",
    -1,
    mcas_methods,
    NULL, NULL, NULL, NULL
};

PyMODINIT_FUNC
PyInit_mcas(void)
{  
  PyObject *m;

  PLOG("Init mcas Python extension");

  import_array();
  
  ZcStringType.tp_base = 0; // no inheritance
  if(PyType_Ready(&ZcStringType) < 0) {
    assert(0);
    return NULL;
  }

  SessionType.tp_base = 0; // no inheritance
  if(PyType_Ready(&SessionType) < 0) {
    assert(0);
    return NULL;
  }

  PoolType.tp_base = 0; // no inheritance
  if(PyType_Ready(&PoolType) < 0) {
    assert(0);
    return NULL;
  }


  /* register module */
#if PY_MAJOR_VERSION >= 3
  m = PyModule_Create(&mcas_module);
#else
#error "Extension for Python 3 only."
#endif

  if (m == NULL)
    return NULL;

  /* add types */
  int rc;

  Py_INCREF(&ZcStringType);
  rc = PyModule_AddObject(m, "ZcString", (PyObject *) &ZcStringType);
  if(rc) return NULL;

  Py_INCREF(&SessionType);
  rc = PyModule_AddObject(m, "Session", (PyObject *) &SessionType);
  if(rc) return NULL;
  
  Py_INCREF(&PoolType);
  rc = PyModule_AddObject(m, "Pool", (PyObject *) &PoolType);
  if(rc) return NULL;


  return m;
}

