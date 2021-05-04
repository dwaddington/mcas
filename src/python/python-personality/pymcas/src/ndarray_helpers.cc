#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION

#include <common/utils.h>
#include <common/dump_utils.h>
#include <Python.h>
#include <numpy/arrayobject.h>

namespace global
{
unsigned debug_level = 3;
}

/** 
 * Create header for array, return as string
 * 
 */
static void create_ndarray_header(PyArrayObject * src_ndarray, std::string& out_hdr)
{
  std::stringstream hdr;

  /* number of dimensions */
  int ndims = PyArray_NDIM(src_ndarray);
  hdr.write(reinterpret_cast<const char*>(&ndims), sizeof(ndims));

  /* item size */
  npy_intp item_size = PyArray_ITEMSIZE(src_ndarray);
  hdr.write(reinterpret_cast<const char*>(&item_size), sizeof(item_size));

  /* dimensions */
  npy_intp * dims = PyArray_DIMS(src_ndarray);
  hdr.write(reinterpret_cast<const char*>(dims), sizeof(npy_intp) * ndims);

  /* strides */
  npy_intp * strides = PyArray_STRIDES(src_ndarray);
  hdr.write(reinterpret_cast<const char*>(strides), sizeof(npy_intp) * ndims);

  if(global::debug_level > 1) {
    PLOG("saving ndims=%d", ndims);
    for(int d=0; d < ndims; d++)
      PLOG("dim=%ld", dims[d]);
  }

  /* selected flags */
  int select_flags =
    PyArray_IS_C_CONTIGUOUS(src_ndarray) |
    PyArray_IS_F_CONTIGUOUS(src_ndarray) |
    PyArray_ISALIGNED(src_ndarray);

  hdr.write(reinterpret_cast<const char*>(&select_flags), sizeof(select_flags));

  /* typenum */
  int type = PyArray_TYPE(src_ndarray);

  if(global::debug_level > 1)
    PLOG("saving type=%d (size=%lu), flags=%d", type, sizeof(type), select_flags);
          
  hdr.write(reinterpret_cast<const char*>(&type), sizeof(type));

  out_hdr = hdr.str();

  if(global::debug_level > 1) {
    PLOG("ndarray with metadata header:");
    hexdump(out_hdr.c_str(), out_hdr.length());
  }
}

PyObject * pymcas_ndarray_header(PyObject * self,
                                 PyObject * args,
                                 PyObject * kwargs)
{
  static const char *kwlist[] = {"source",
                                 NULL};

  PyObject * src_obj = nullptr;

  if (! PyArg_ParseTupleAndKeywords(args,
                                    kwargs,
                                    "O",
                                    const_cast<char**>(kwlist),
                                    &src_obj)) {
    PyErr_SetString(PyExc_RuntimeError,"bad arguments");
    return NULL;
  }

  if (! PyArray_Check(src_obj)) {
    PyErr_SetString(PyExc_RuntimeError,"not ndarray type");
    return NULL;
  }

  PyArrayObject * src_ndarray = reinterpret_cast<PyArrayObject*>(src_obj);

  /* sanity checks */
  if (! PyArray_ISBEHAVED(src_ndarray)) {
    PyErr_SetString(PyExc_RuntimeError,"un-behaving ndarray type not supported");
    return NULL;
  }

  if (! PyArray_ISONESEGMENT(src_ndarray)) {
    PyErr_SetString(PyExc_RuntimeError,"only single-segment ndarray supported");
    return NULL;
  }

  std::string hdr;
  create_ndarray_header(src_ndarray, hdr);
  
  return PyByteArray_FromStringAndSize(hdr.c_str(), hdr.size());
}

PyObject * unmarshall_nparray(byte * ptr)
{
  import_array();
  
  int ndims = *(reinterpret_cast<npy_intp*>(ptr));
  ptr += sizeof(ndims);

  npy_intp item_size = *(reinterpret_cast<npy_intp*>(ptr));
  ptr += sizeof(item_size);

  std::vector<npy_intp> dims;
  for(int i=0; i < ndims; i++) {
    npy_intp dim = *(reinterpret_cast<npy_intp*>(ptr));
    ptr += sizeof(dim);
    dims.push_back(dim);
  }

  std::vector<npy_intp> strides;
  for(int i=0; i < ndims; i++) {
    npy_intp stride = *(reinterpret_cast<npy_intp*>(ptr));
    ptr += sizeof(stride);
    strides.push_back(stride);
  }

  int flags = *(reinterpret_cast<int*>(ptr));
  ptr += sizeof(flags);

  assert(flags == 1);
  
  int type =  *(reinterpret_cast<int*>(ptr));
  ptr += sizeof(type);

  if(global::debug_level > 2) {
    PLOG("ndims=%d, flags=%d, type=%d", ndims, flags, type);
    for(auto d: dims) PLOG("dim=%ld", d);
    for(auto s: strides) PLOG("stride=%ld", s);
  }

  PyObject* nparray = PyArray_New(&PyArray_Type,
                                  ndims,
                                  dims.data(),
                                  type,
                                  strides.data(),
                                  ptr, // TO DO check with header length?
                                  item_size,
                                  flags,
                                  NULL);
  PLOG("PyArray_New OK");
  return nparray;
}

PyObject * pymcas_ndarray_from_bytes(PyObject * self,
                                     PyObject * args,
                                     PyObject * kwargs)
{
  static const char *kwlist[] = {"data",
                                 "header_length",
                                 NULL};

  PyObject * bytes_memory_view  = nullptr;
  Py_ssize_t header_length = 0;

  if (! PyArg_ParseTupleAndKeywords(args,
                                    kwargs,
                                    "On",
                                    const_cast<char**>(kwlist),
                                    &bytes_memory_view,
                                    &header_length)) {
    PyErr_SetString(PyExc_RuntimeError,"bad arguments!");
    return NULL;
  }

  if (! PyMemoryView_Check(bytes_memory_view)) {
    PyErr_SetString(PyExc_RuntimeError,"data should be type <memoryview>");
    return NULL;
  }


  Py_INCREF(bytes_memory_view); /* increment reference count to hold data */

  Py_buffer * buffer = PyMemoryView_GET_BUFFER(bytes_memory_view);
  byte * ptr = (byte *) buffer->buf;

  if(global::debug_level > 1) {
    PLOG("header_length = %lu", header_length);
    hexdump(ptr, header_length);
  }

  /* unmarsh the ndarray from metadata - zero copy */
  auto nparray = unmarshall_nparray(ptr);
  
  return nparray;
}
