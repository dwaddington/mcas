#include <assert.h>
#include <stdlib.h>
#include <common/logging.h>
#include <common/utils.h>
#include <api/mcas_itf.h>
#include <api/kvstore_itf.h>
#include <Python.h>
#include <structmember.h>

/* defaults */
constexpr const char * DEFAULT_PMEM_PATH = "/mnt/pmem0";
constexpr const char * DEFAULT_POOL_NAME = "default";
constexpr uint64_t DEFAULT_LOAD_ADDR     = 0x900000000;

typedef struct {
  PyObject_HEAD
  component::IKVStore * _store;
  component::IKVStore::pool_t _pool;
} MemoryResource;

static PyObject *
MemoryResource_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  auto self = (MemoryResource *)type->tp_alloc(type, 0);
  assert(self);
  return (PyObject*)self;
}

/** 
 * tp_dealloc: Called when reference count is 0
 * 
 * @param self 
 */
static void
MemoryResource_dealloc(MemoryResource *self)
{
  assert(self);

  PLOG("MemoryResource: dealloc (%p)", self);

  self->_store->close_pool(self->_pool);
  
  if(self->_store)
    self->_store->release_ref();
  
  assert(self);
  Py_TYPE(self)->tp_free((PyObject*)self);
}

static component::IKVStore * load_backend(const std::string& backend,
                                          const std::string& path,
                                          const uint64_t load_addr,
                                          const unsigned debug_level)
{
  using namespace component;
  
  IBase* comp = nullptr;
  if(backend == "hstore") {
    comp = load_component("libcomponent-hstore.so", hstore_factory);
  }
  else if (backend == "hstore-cc") {
    comp = load_component("libcomponent-hstore-cc.so", hstore_factory);
  }
  else assert(0);

  IKVStore* store = nullptr;
  auto fact = make_itf_ref(static_cast<IKVStore_factory *>(comp->query_interface(IKVStore_factory::iid())));
  assert(fact);

  /* TODO configure from params */
  std::stringstream ss;
  ss << "[{\"path\":\"" << path << "\",\"addr\":" << load_addr << "}]";
  //  PLOG("dax config: %s", ss.str().c_str());
  
  store = fact->create(debug_level,
    {
     {+component::IKVStore_factory::k_debug, std::to_string(debug_level)},
     {+component::IKVStore_factory::k_dax_config, ss.str()} //dax_config}
    });
  
  return store;
}

static int MemoryResource_init(MemoryResource *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"pool_name",
                                 "size_mb",
                                 "pmem_path",
                                 "load_addr",
                                 "debug",
                                 NULL,
  };

  const char * p_pool_name = nullptr;
  int size_mb = 32;
  const char * p_path = nullptr;
  const char * p_addr = nullptr;
  int debug_level = 3;

  if (! PyArg_ParseTupleAndKeywords(args,
                                    kwds,
                                    "|sissi",
                                    const_cast<char**>(kwlist),
                                    &p_pool_name,
                                    &p_path,
                                    &p_addr,
                                    &size_mb,
                                    &debug_level)) {
    PyErr_SetString(PyExc_RuntimeError, "bad arguments");
    PWRN("bad arguments or argument types to MemoryResource constructor");
    return -1;
  }

  uint64_t load_addr = DEFAULT_LOAD_ADDR;
  if(p_addr)
    load_addr = ::strtoul(p_addr,NULL,16);
  
  std::string pool_name = p_pool_name ? p_pool_name : DEFAULT_POOL_NAME;
  std::string path = p_path ? p_path : DEFAULT_PMEM_PATH;  

  self->_store = load_backend("hstore-cc", path, load_addr, debug_level);
  assert(self->_store);

  if((self->_pool = self->_store->create_pool(pool_name, MiB(32))) == 0) {
    PyErr_SetString(PyExc_RuntimeError, "unable to create/open pool");
    return -1;
  }
  
  return 0;
}

static PyObject * MemoryResource_get_named_memory(PyObject * self,
                                                  PyObject * args,
                                                  PyObject * kwds)
{
  static const char *kwlist[] = {"name",
                                 NULL};

  char * name = nullptr;
  
  if (! PyArg_ParseTupleAndKeywords(args,
                                    kwds,
                                    "s|",
                                    const_cast<char**>(kwlist),
                                    &name)) {
    PyErr_SetString(PyExc_RuntimeError,"bad arguments");
    return NULL;
  }
  
  char * ptr = static_cast<char*>(aligned_alloc(PAGE_SIZE, nsize));

  if(zero_flag)
    memset(ptr, 0x0, nsize);
  
  if(ptr == NULL) {
    PyErr_SetString(PyExc_RuntimeError,"aligned_alloc failed");
    return NULL;
  }

  memset(ptr, 0xe, nsize); // temporary
  PNOTICE("%s allocated %lu at %p", __func__, nsize, ptr);
  return PyMemoryView_FromMemory(ptr, nsize, PyBUF_WRITE);
}


static PyMemberDef MemoryResource_members[] =
  {
   //  {"port", T_ULONG, offsetof(MemoryResource, _port), READONLY, "Port"},
   {NULL}
  };


//MemoryResource_get_named_memory
static PyMethodDef MemoryResource_methods[] =
  {
   {"MemoryResource_get_named_memory",  (PyCFunction) MemoryResource_get_named_memory, METH_VARARGS | METH_KEYWORDS, "get named memory"},
   // {"create_pool",  (PyCFunction) create_pool, METH_VARARGS | METH_KEYWORDS, create_pool_doc},
   // {"delete_pool",  (PyCFunction) delete_pool, METH_VARARGS | METH_KEYWORDS, delete_pool_doc},
   // {"get_stats",  (PyCFunction) get_stats, METH_VARARGS | METH_KEYWORDS, get_stats_doc},
   {NULL}
  };


PyTypeObject MemoryResourceType = {
  PyVarObject_HEAD_INIT(NULL, 0)
  "pymm.MemoryResource",           /* tp_name */
  sizeof(MemoryResource)   ,      /* tp_basicsize */
  0,                       /* tp_itemsize */
  (destructor) MemoryResource_dealloc,      /* tp_dealloc */
  0,                       /* tp_print */
  0,                       /* tp_getattr */
  0,                       /* tp_setattr */
  0,                       /* tp_reserved */
  0,                       /* tp_repr */
  0,                       /* tp_as_number */
  0,                       /* tp_as_sequence */
  0,                       /* tp_as_mapping */
  0,                       /* tp_hash */
  0,                       /* tp_call */
  0,                       /* tp_str */
  0,                       /* tp_getattro */
  0,                       /* tp_setattro */
  0,                       /* tp_as_buffer */
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
  "MemoryResource",              /* tp_doc */
  0,                       /* tp_traverse */
  0,                       /* tp_clear */
  0,                       /* tp_richcompare */
  0,                       /* tp_weaklistoffset */
  0,                       /* tp_iter */
  0,                       /* tp_iternext */
  MemoryResource_methods,         /* tp_methods */
  MemoryResource_members,         /* tp_members */
  0,                       /* tp_getset */
  0,                       /* tp_base */
  0,                       /* tp_dict */
  0,                       /* tp_descr_get */
  0,                       /* tp_descr_set */
  0,                       /* tp_dictoffset */
  (initproc)MemoryResource_init,  /* tp_init */
  0,            /* tp_alloc */
  MemoryResource_new,             /* tp_new */
  0, /* tp_free */
};
