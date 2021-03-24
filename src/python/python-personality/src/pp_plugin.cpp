/*
   Copyright [2021] [IBM Corporation]
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
#include <common/logging.h>
#include <common/utils.h>
#include <common/dump_utils.h>
#include <common/cycles.h>
#include <api/interfaces.h>
#include <api/ado_itf.h>
#include <stdlib.h>
#include <string.h>
#include <flatbuffers/flatbuffers.h>


#include <Python.h>

#define MAGIC 0x0c0ffee0
#define VERSION 1
#define PLUGIN_VERSION "0.1"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wconversion-null"

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#pragma GCC diagnostic push

#include "pp_generated.h"
#include "pp_plugin.h"

#define PREFIX "Pp_plugin: "
using namespace flatbuffers;

constexpr const uint64_t CANARY = 0xCAFEF001;
int debug_level = 3;

/* externs - from pymcas_core_module.cc */
extern PyObject * unmarshall_nparray(byte * ptr); 
extern void create_ndarray_header(PyArrayObject * src_ndarray, std::string& out_hdr);


inline void * copy_flat_buffer(FlatBufferBuilder& fbb)
{
  auto fb_len = fbb.GetSize();
  void * ptr = ::malloc(fb_len);
  memcpy(ptr, fbb.GetBufferPointer(), fb_len);
  //  hexdump(ptr, fb_len);
  return ptr;
}

/*------------------------------------------------------------*/
/* extension for ADO-local python access to ADO callback etc. */
/*------------------------------------------------------------*/
// We might need this to support ADO callbacks from python code
// static PyObject* ado_ext_foo(PyObject *self, PyObject *args)
// {
//   PNOTICE("ado_ext_foo!!!!");
//   return PyLong_FromLong(99);
// }

// static PyMethodDef AdoExtMethods[] = {
//   {"foo", ado_ext_foo, METH_VARARGS, "Do something."},
//   {NULL, NULL, 0, NULL}
// };

// static PyModuleDef AdoExtModule = {
//   PyModuleDef_HEAD_INIT, "ado", NULL, -1, AdoExtMethods,
//   NULL, NULL, NULL, NULL
// };

// static PyObject*
// PyInit_ado(void)
// {
//   return PyModule_Create(&AdoExtModule);
// }



status_t Pp_plugin::register_mapped_memory(void * shard_vaddr,
                                                  void * local_vaddr,
                                                  size_t len)
{
  CPLOG(2, PREFIX "register_mapped_memory (%p, %p, %lu)",
       shard_vaddr, local_vaddr, len);
  /* we would need a mapping if we are not using the same virtual
     addresses as the Shard process */

  return S_OK;
}

Pp_plugin::Pp_plugin() : common::log_source(DEBUG_LEVEL)
{
  PINF("Plugin: Python Personality - Version " PLUGIN_VERSION);
  PINF("ADO userid: %d", getuid());

  /* enable this to debug module loading */
  //  setenv("PYTHONVERBOSE","9",1);
  //  Py_SetProgramName(Py_DecodeLocale("/usr/bin/python3.6", NULL));

  PINF("Python Path: %ls", Py_GetPath());
  PINF("Python version: %s", Py_GetVersion());

  // TODO configure with .conf file
  dlopen("/usr/lib64/libpython3.so", RTLD_LAZY | RTLD_GLOBAL);

  /* add methods to module ado */
  //  PyImport_AppendInittab("ado", &PyInit_ado);
  
  Py_Initialize();
  //  PyRun_SimpleString("import ado; ado.foo()"); // import local extension
  //PyRun_SimpleString("import sys; print(sys.modules.keys())"); // ado.foo()");
  //  PyRun_SimpleString("import skimage");
}


status_t Pp_plugin::do_work(const uint64_t work_key,
                            const char * key,
                            size_t key_len,
                            IADO_plugin::value_space_t& values,
                            const void *in_work_request,
                            const size_t in_work_request_len,
                            bool new_root,
                            response_buffer_vector_t& response_buffers)
{
  CPLOG(1, PREFIX "do work key=(%.*s)", (int) key_len, key);
  CPLOG(1, PREFIX "do work in_work_request_len=%ld", in_work_request_len);

  import_array();
  
  void * value = values[0].ptr;
  size_t value_len = values[0].len;
  
  /* protocol interpretation */
  const char * code = nullptr;
  const char * function = nullptr;
  {
    using namespace Proto;
    auto msg = GetSizePrefixedMessage(in_work_request);

    CPLOG(2, PREFIX "magic=%x version=%d", msg->magic(), msg->version());
    assert(msg->magic() == 0xc0ffee0);
    assert(msg->version() == 1);
    
    const InvokeRequest * ir;
    if((ir = msg->element_as_InvokeRequest())) {
      auto op = ir->op();
      code = op->code()->c_str();
      function = op->function()->c_str();
    }
  }

  auto rc = execute_python(work_key,
                           key,
                           key_len,
                           value,
                           value_len,
                           code,
                           function,
                           response_buffers);

  /* create flatbuffer response */
  {
    using namespace Proto;
    
    FlatBufferBuilder fbb;

    auto req = CreateInvokeReply(fbb, rc);
    fbb.Finish(CreateMessage(fbb, MAGIC, VERSION, Element_InvokeReply, req.Union()));
    response_buffers.emplace_back(copy_flat_buffer(fbb),
                                  fbb.GetSize(),
                                  response_buffer_t::alloc_type_malloc{});
  }
  return S_OK;
}

/** 
 * Unwrap ndarray from DataDescriptor
 * 
 * @param value 
 * @param value_len 
 * 
 * @return 
 */
status_t Pp_plugin::unwrap_nparray_from_data_descriptor(const char * key,
                                                        const size_t key_len,
                                                        void *       value,
                                                        const size_t value_len,
                                                        PyObject *&  out_ndarray)
{
  using namespace Proto;
  auto msg = GetSizePrefixedMessage(value);
  uint32_t msg_size = *(static_cast<uint32_t*>(value));

  CPLOG(3, PREFIX "FB header size: %u", msg_size);

  const DataDescriptor * desc;
  if((desc = msg->element_as_DataDescriptor())) {
    
    const char * global_name = desc->global_name()->c_str();
    CPLOG(3, PREFIX "desc: global_name (%s)", global_name);

    /* check global name */
    if(memcmp(key, global_name, key_len) > 0)
      return E_INVAL;

    if(desc->type() != Proto::DataType::DataType_NumPyArray)
      return E_INVAL;

    byte * data = static_cast<byte*>(value) + msg_size + 4; /* skip FB header, 4B prefix */
    out_ndarray = unmarshall_nparray(data);

    return S_OK;
  }

  return E_FAIL;
}


status_t Pp_plugin::unwrap_pickle_from_data_descriptor(const char * key,
                                                       const size_t key_len,
                                                       void *       value,
                                                       const size_t value_len,
                                                       PyObject *&  out_object)
{
  using namespace Proto;
  auto msg = GetSizePrefixedMessage(value);
  uint32_t msg_size = *(static_cast<uint32_t*>(value));

  CPLOG(3, PREFIX "FB header size: %u", msg_size);

  const DataDescriptor * desc;
  if((desc = msg->element_as_DataDescriptor())) {
    
    const char * global_name = desc->global_name()->c_str();
    CPLOG(3, PREFIX "desc: global_name (%s)", global_name);

    /* check global name */
    if(memcmp(key, global_name, key_len) > 0)
      return E_INVAL;

    if(desc->type() != Proto::DataType::DataType_Pickled)
      return E_INVAL;

    char * data = static_cast<char*>(value) + msg_size + 4; /* skip FB header, 4B prefix */
    out_object = PyMemoryView_FromMemory(data, value_len - msg_size - 4, PyBUF_READ);
    
    return S_OK;
  }

  return E_FAIL;
}


void Pp_plugin::write_to_store(const uint64_t work_id,
                               const char *   varname,
                               PyObject *     object)
{
  CPLOG(2, PREFIX "Writing to store object (%s)", varname);

  std::string key(varname);

  if(PyArray_Check(object)) {

    auto array_object = reinterpret_cast<PyArrayObject*>(object);
    auto array_size = PyArray_NBYTES(array_object);
    /* create ndarray header */
    std::string nd_header;
    create_ndarray_header(array_object, nd_header);
    CPLOG(2, PREFIX "write_to_store: nd_header size=%lu", nd_header.size());

    /* assemble flatbuffer header */
    {
      using namespace Proto;
      
      FlatBufferBuilder fbb;
      
      auto global_name = fbb.CreateString(varname);
      auto dd = CreateDataDescriptor(fbb,
                                     DataType_NumPyArray,
                                     global_name,
                                     nd_header.size(),
                                     array_size);
      
      fbb.FinishSizePrefixed(CreateMessage(fbb,
                                           MAGIC,
                                           VERSION,
                                           Element_DataDescriptor,
                                           dd.Union()));

      auto fb_header_size = fbb.GetSize();
      auto fb_header_ptr = fbb.GetBufferPointer();
      CPLOG(2, PREFIX "write_to_store: fb msg size: %u", fb_header_size);
      size_t value_size = fb_header_size + nd_header.size() + array_size;
      CPLOG(2, PREFIX "write_to_store: value_size: %lu", value_size);
      
      /* for the moment copy */
      const size_t requested_value_size = value_size;
      void * out_value_addr = nullptr;
      
      component::IKVStore::key_t key_handle;
      status_t rc = cb_create_key(work_id,
                                  key,
                                  value_size,
                                  IADO_plugin::FLAGS_CREATE_ONLY, // flags
                                  out_value_addr,
                                  nullptr,
                                  &key_handle);

      if(rc != S_OK) {
        if(rc == E_ALREADY_EXISTS) {
          CPLOG(2, PREFIX "value already exists; resizing..");

          out_value_addr = nullptr;
          if(value_size != requested_value_size) {
            if(cb_resize_value(work_id, key, requested_value_size, out_value_addr) != S_OK)
              throw General_exception("resizing value failed");
          }
          else {
            /* just open it, its the right size */
            if(cb_open_key(work_id,
                           key,
                           out_value_addr,
                           value_size,
                           nullptr,
                           &key_handle) != S_OK)
              throw General_exception("opening existing key");
          }

          assert(out_value_addr != nullptr);
          assert(value_size == requested_value_size);               
        }
        else throw General_exception("create_key ADO callback failed");
      }

      assert(out_value_addr);
      auto ptr = static_cast<byte *>(out_value_addr);
      memcpy(ptr, fb_header_ptr, fb_header_size);
      ptr+=fb_header_size;
      memcpy(ptr, nd_header.c_str(), nd_header.size());
      ptr+=nd_header.size();
      memcpy(ptr, PyArray_DATA(array_object), array_size);
    }
  }
  else {
    /* handle pickled data form */
    auto local_dict = PyDict_New();
    auto global_dict = PyDict_New();
    PyDict_SetItemString(global_dict, "__builtins__", PyEval_GetBuiltins());
    PyDict_SetItemString(global_dict, varname, object);

    std::string pcode = "import pickle; pickled_bytes = pickle.dumps(";
    pcode += varname;
    pcode += ")\n";
    PyRun_String(pcode.c_str(), Py_file_input, global_dict, local_dict);
    PyObject * pickled_bytes = PyDict_GetItemString(local_dict, "pickled_bytes");

    if (PyErr_Occurred()) {
      PyErr_Print();
      throw General_exception("execution of unpickle code failed unexpected");
    }

    if(!pickled_bytes || ! PyBytes_Check(pickled_bytes))
      throw General_exception("unexpected non-array type from pickle");

    /* assemble flatbuffer header */
    {
      using namespace Proto;
      
      FlatBufferBuilder fbb;

      auto pickled_bytes_len = PyBytes_Size(pickled_bytes);
      PNOTICE("pickled_bytes_len %lu", pickled_bytes_len);
      auto global_name = fbb.CreateString(varname);
      auto dd = CreateDataDescriptor(fbb,
                                     DataType_Pickled,
                                     global_name,
                                     0,
                                     pickled_bytes_len);
      
      fbb.FinishSizePrefixed(CreateMessage(fbb,
                                           MAGIC,
                                           VERSION,
                                           Element_DataDescriptor, dd.Union()));

      auto fb_header_size = fbb.GetSize();
      auto fb_header_ptr = fbb.GetBufferPointer();
      
      CPLOG(2, PREFIX "write_to_store: fb msg size=%u pickled_bytes_len=%lu",
            fb_header_size, pickled_bytes_len);
      
      size_t value_size = fb_header_size + pickled_bytes_len;
      const size_t requested_value_size = value_size;
      
      CPLOG(2, PREFIX "write_to_store: creating (key=%s) value of value_size=%lu",
            key.c_str(), value_size);
      
      /* for the moment memory copy */
      void * out_value_addr = nullptr;

      // TEST CODE (test stomping)
      // cb_create_key(work_id,
      //               key,
      //               99,
      //               FLAGS_NO_IMPLICIT_UNLOCK, // flags
      //               out_value_addr);

      component::IKVStore::key_t key_handle;
      status_t rc = cb_create_key(work_id,
                                  key,
                                  value_size,
                                  IADO_plugin::FLAGS_CREATE_ONLY, // flags
                                  out_value_addr,
                                  nullptr,
                                  &key_handle);

      if(rc != S_OK) {
        if(rc == E_ALREADY_EXISTS) {
          CPLOG(2, PREFIX "value already exists; resizing..");

          out_value_addr = nullptr;
          if(value_size != requested_value_size) {
            if(cb_resize_value(work_id, key, requested_value_size, out_value_addr) != S_OK)
              throw General_exception("resizing value failed");
          }
          else {
            /* just open it, its the right size */
            if(cb_open_key(work_id,
                           key,
                           out_value_addr,
                           value_size,
                           nullptr,
                           &key_handle) != S_OK)
              throw General_exception("opening existing key");
          }

          assert(out_value_addr != nullptr);
          assert(value_size == requested_value_size);               
        }
        else throw General_exception("create_key ADO callback failed");
      }

      assert(out_value_addr);
      auto ptr = static_cast<byte *>(out_value_addr);
      memcpy(ptr, fb_header_ptr, fb_header_size);
      ptr+=fb_header_size;
      memcpy(ptr, PyBytes_AS_STRING(pickled_bytes), pickled_bytes_len);
      /* flush for PMEM? */
      cb_unlock(work_id, key_handle);  /* explicitly unlock */
    }

  }

}

status_t Pp_plugin::execute_python(const uint64_t              work_key,
                                   const char *                key,
                                   const size_t                key_len,
                                   void *                      value,
                                   const size_t                value_len,
                                   const char *                code_string,
                                   const char *                function_name,
                                   response_buffer_vector_t&   response_buffers)
{
  
  // PLOG("Importing pickle..");
  // PyObject *mod_pickle = PyImport_ImportModule("pickle");
  // if(mod_pickle == nullptr)
  //   throw General_exception("unable to import pickle");

  PyObject *mod_numpy = PyImport_ImportModule("numpy");
  if(mod_numpy == nullptr)
    throw General_exception("unable to import numpy");


  std::string code(code_string);

  /* set up environment */
  auto local_dict = PyDict_New();
  auto global_dict = PyDict_New();
  PyDict_SetItemString(global_dict, "__builtins__", PyEval_GetBuiltins());

  PyObject * target_object = nullptr;
  status_t rc = unwrap_nparray_from_data_descriptor(key, key_len, value, value_len, target_object);

  if(target_object == nullptr) {

    /* target is a pickled object */
    PyObject * pickle_bytes = nullptr;
    rc = unwrap_pickle_from_data_descriptor(key, key_len, value, value_len, pickle_bytes);
    
    if(rc != S_OK)
      throw General_exception("not ndarray or pickle; what to do?");

    PyDict_SetItemString(global_dict, "pickle_bytes", pickle_bytes);

    std::string pcode = "import pickle; unpickled_object = pickle.loads(pickle_bytes)\n";
    PyRun_String(pcode.c_str(), Py_file_input, global_dict, local_dict);

    if (PyErr_Occurred()) {
      PyErr_Print();
      throw General_exception("execution of unpickle code failed unexpected");
    }

    target_object = PyDict_GetItemString(local_dict, "unpickled_object");
  }

  assert(target_object);
  PyDict_SetItemString(global_dict, "target", target_object);


  /* add execution hook, i.e. call target function with params */
  code += "\nresults = ";
  code += function_name;
  code += "(target)\n";

  CPLOG(2, PREFIX "JIT compiling code..\n%s",code.c_str());
    
  /* JIT compile code */
  auto cc = Py_CompileString(code.c_str(), "jitcode", Py_file_input);
  if(!cc)
    throw General_exception("unable to JIT compile python code");
  
  /* execute code */
  PyObject* result = PyEval_EvalCode(cc, global_dict, local_dict);
  if(!result)
    throw General_exception("PyEval_EvalCode failed unexpectedly");

  CPLOG(2, PREFIX "Python execution OK");


  if(debug_level() > 1)
  { /* debugging only */
    PyObject *k, *v;
    Py_ssize_t pos = 0;
    
    while (PyDict_Next(local_dict, &pos, &k, &v))
      PINF("local variable: %ls", PyUnicode_AsWideCharString(k, NULL));

    pos = 0;
    while (PyDict_Next(global_dict, &pos, &k, &v))
      PINF("global variable: %ls", PyUnicode_AsWideCharString(k, NULL));
  }
  
  PyObject * results = PyDict_GetItemString(local_dict, "results");
  if(! PyDict_Check(results))
    throw General_exception("results expected to be a dictionary");


  /* write results back into store */
  Py_ssize_t pos = 0;
  PyObject *varname, *var;
  while (PyDict_Next(results, &pos, &varname, &var)) {

    char * utf8_varname = PyUnicode_AsUTF8(varname);
    write_to_store(work_key, utf8_varname, var);

    PyMem_Free(utf8_varname);
  }
  
  
  return rc;
}


/* called when the pool is opened and the ADO is launched */
void Pp_plugin::launch_event(const uint64_t                  auth_id,
                             const std::string&              pool_name,
                             const size_t                    pool_size,
                             const unsigned int              pool_flags,
                             const unsigned int              memory_type,
                             const size_t                    expected_obj_count,
                             const std::vector<std::string>& params)
{
}


/* called just before ADO shutdown */
status_t Pp_plugin::shutdown()
{
  Py_Finalize();
  return S_OK;
}




/**
 * Factory-less entry point
 *
 */
extern "C" void * factory_createInstance(component::uuid_t interface_iid)
{
  if(interface_iid == interface::ado_plugin)
    return static_cast<void*>(new Pp_plugin());
  else return NULL;
}

