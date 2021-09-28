/*
  Copyright [2017-2021] [IBM Corporation]
  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at
  http://www.apache.org/licenses/LICENSE-2.0
  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#define NO_IMPORT_ARRAY
#define PY_ARRAY_UNIQUE_SYMBOL pymmcore_ARRAY_API

#include <vector>
#include <assert.h>
#include <stdlib.h>
#include <common/logging.h>
#include <common/utils.h>
#include <common/dump_utils.h>

#include <Python.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#include <numpy/arrayobject.h>
#pragma GCC diagnostic pop

#include "metadata.h"
#include "dlpack.h"

typedef int tvm_index_t;


#if 0
static size_t dlpack_get_data_size(DLDataType dtype, int ndim) {
   size_t size = 1;
   for (tvm_index_t i = 0; i < ndim; ++i) {
     size *= t->shape[i];
   }
   size *= (t->dtype.bits * t->dtype.lanes + 7) / 8;
  return size;
}

static size_t dlpack_get_DLTensor_size(int ndim) {
  size_t nbytes = sizeof(DLTensor);
  nbytes += sizeof(int64_t) * ndim * 2;
  nbytes += dlpack_get_data_size
  return 0;
}
#endif

/** 
 * Takes an ndarray (param ndarray) and determines required memory
 * 
 * @param self 
 * @param args 
 * @param kwds 
 * 
 * @return Size of required memory in bytes
 */
static PyObject * pymmcore_dlpack_calculate_size(PyObject * self,
                                                 PyObject * args,
                                                 PyObject * kwargs)
{
  static const char *kwlist[] = {"ndarray",
                                 NULL};

  PyObject * array_obj = nullptr;
  const char * dtype_str = nullptr;
  int type = 0;

  if (! PyArg_ParseTupleAndKeywords(args,
                                    kwargs,
                                    "O",
                                    const_cast<char**>(kwlist),
                                    &array_obj)) {
    PyErr_SetString(PyExc_RuntimeError,"bad arguments");
    return NULL;
  }

  if (! PyArray_Check(array_obj)) {
    PyErr_SetString(PyExc_RuntimeError,"not ndarray type");
    return NULL;
  }

  PyArrayObject * ndarray = reinterpret_cast<PyArrayObject*>(array_obj);

  /* sanity checks */
  if (! PyArray_ISBEHAVED(ndarray)) {
    PyErr_SetString(PyExc_RuntimeError,"misbehaving ndarray type not supported");
    return NULL;
  }

  if (! PyArray_ISONESEGMENT(ndarray)) {
    PyErr_SetString(PyExc_RuntimeError,"only single-segment ndarray supported");
    return NULL;
  }


  Py_RETURN_NONE;

}


PyObject * pymmcore_dlpack_construct_meta(PyObject * self,
                                          PyObject * args,
                                          PyObject * kwargs)
{
  static const char *kwlist[] = {"dtypedescr",
                                 "shape",
                                 "strides",
                                 NULL};

  PyObject * dtypedescr_obj = nullptr;
  PyObject * shape_obj = nullptr;
  PyObject * strides_obj = nullptr;

  if (! PyArg_ParseTupleAndKeywords(args,
                                    kwargs,
                                    "OOO",
                                    const_cast<char**>(kwlist),
                                    &dtypedescr_obj,
                                    &shape_obj,
                                    &strides_obj)) {
    PyErr_SetString(PyExc_RuntimeError,"bad arguments");
    return NULL;
  }

  if (! PyArray_DescrCheck(dtypedescr_obj)) {
    PyErr_SetString(PyExc_RuntimeError,"bad type of dtypedescr parameter");
    return NULL;
  }

  /* construct:
     [ MetaHeader | int ndim | DLDataType dtype | int64_t shape[ndim] | int64_t strides[ndim] ]
  */

  /* convert from numpy type to dlpack type */
  DLDataType ddt;
  ddt.lanes = 1; /* no vectorization */
  
  auto dtypedescr = reinterpret_cast<PyArray_Descr*>(dtypedescr_obj);
  switch(dtypedescr->kind) {
  case 'i': // signed int
    ddt.code = kDLInt;
    break;
  case 'u': // unsigned int
    ddt.code = kDLUInt;
    break;
  case 'f': // float
    ddt.code = kDLFloat;
    break;
  case 'c': // complex
    ddt.code = kDLComplex;
  default:
    PyErr_SetString(PyExc_RuntimeError,"unsupport type");
    return NULL;
  }
  ddt.bits = dtypedescr->elsize * 8;

  PLOG("dtypedescr.type_num = %d", dtypedescr->type_num);
  PLOG("ddt.code = %u", ddt.code);

  /* handle shape */
  std::vector<unsigned long> c_shape;
  if (PyList_Check(shape_obj)) {
    Py_ssize_t idx = 0;
    PyObject * element;
    while((element = PyList_GetItem(shape_obj, idx))) {
      unsigned long n = PyLong_AsUnsignedLong(element);
      if (n == static_cast<unsigned long>(-1L)) {
        PyErr_SetString(PyExc_RuntimeError,"bad shape element");
        return NULL;
      }
      c_shape.push_back(n);
      idx++;
    }
  }
  else if (PyTuple_Check(shape_obj)) {
    Py_ssize_t idx = 0;
    PyObject * element;
    while((element = PyTuple_GetItem(shape_obj, idx))) {
      unsigned long n = PyLong_AsUnsignedLong(element);
      if (n == static_cast<unsigned long>(-1L)) {
        PyErr_SetString(PyExc_RuntimeError,"bad shape element");
        return NULL;
      }
      c_shape.push_back(n);
      idx++;
    }
  }
  else {
    PyErr_SetString(PyExc_RuntimeError,"shape should be list or tuple");
    return NULL;
  }

  /* handle strides */
  std::vector<unsigned long> c_strides;
  if (Py_None == strides_obj) {
  }
  else if (PyList_Check(strides_obj)) {
    Py_ssize_t idx = 0;
    PyObject * element;
    while((element = PyList_GetItem(strides_obj, idx))) {
      unsigned long n = PyLong_AsUnsignedLong(element);
      if (n == static_cast<unsigned long>(-1L)) {
        PyErr_SetString(PyExc_RuntimeError,"bad strides element");
        return NULL;
      }
      c_strides.push_back(n);
      idx++;
    }
  }
  else if (PyTuple_Check(strides_obj)) {
    Py_ssize_t idx = 0;
    PyObject * element;
    while((element = PyTuple_GetItem(strides_obj, idx))) {
      unsigned long n = PyLong_AsUnsignedLong(element);
      if (n == static_cast<unsigned long>(-1L)) {
        PyErr_SetString(PyExc_RuntimeError,"bad strides element");
        return NULL;
      }
      c_strides.push_back(n);
      idx++;
    }
  }
  else {
    PyErr_SetString(PyExc_RuntimeError,"shape should be list or tuple");
    return NULL;
  }

  if(!c_strides.empty() && !c_shape.empty()) {
    if(c_strides.size() != c_shape.size()) {
      PyErr_SetString(PyExc_RuntimeError,"stride and shape mismatch");
      return NULL;
    }
  }
  
  size_t ndim = c_shape.size();
  size_t total_metadat_size = sizeof(MetaHeader) + sizeof(int) + sizeof(DLDataType) + (sizeof(int64_t) * ndim) + (sizeof(int64_t) * ndim);                                                           
  MetaHeader metadata_header;
  metadata_header.magic = HeaderMagic;
  metadata_header.magic = 0;
  metadata_header.version = 0;
  metadata_header.type = DataType_DLPackArray;
  metadata_header.subtype = DataSubType_None;

  /* no construct the header */

  PNOTICE("size=%lu", total_metadat_size);
  PNOTICE("dlpack_construct_meta OK so far!!!");
  Py_RETURN_NONE;
}
