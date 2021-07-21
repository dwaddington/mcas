#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#define PY_ARRAY_UNIQUE_SYMBOL pymmcore_ARRAY_API

#include <common/logging.h>
#include <Python.h>

extern "C" void* Intercept_Malloc(void * ctx, size_t n)
{
  if(n > 1024)
    PLOG("alloc (%lu)", n);
  return malloc(n); //PyMem_RawMalloc(n);
}


extern "C" void* Intercept_Calloc(void * ctx, size_t nelem, size_t elsize)
{
  if(nelem*elsize > 1024)
    PLOG("calloc (%lu)", nelem*elsize);
  return calloc(nelem,elsize); //PyMem_RawMalloc(n);
}

extern "C" void* Intercept_Realloc(void * ctx, void * p, size_t n)
{
  if(n > 1024)
    PLOG("realloc (%lu)", n);
  return realloc(p, n);
}


PyObject * pymmcore_enable_transient_memory(PyObject * self,
                                            PyObject * args,
                                            PyObject * kwargs)
{
  PyMemAllocatorEx allocator;
  PyMem_GetAllocator(PYMEM_DOMAIN_RAW, &allocator);

  allocator.malloc = &Intercept_Malloc;
  allocator.realloc = &Intercept_Realloc;
  allocator.calloc = &Intercept_Calloc;
  // allocator.free = PyObject_Free;
  
  PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &allocator);


  // PyObjectArenaAllocator allocators;    
  // PyObject_GetArenaAllocator(&allocators);
  // PyObject_SetArenaAllocator(&allocators);
  
  Py_RETURN_NONE;
}

