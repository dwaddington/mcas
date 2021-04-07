#define DEFAULT_DEVICE "mlx5_0"
#define DEFAULT_PORT 11911

#include <sstream>
#include <common/logging.h>
#include <api/mcas_itf.h>
#include <api/kvstore_itf.h>
#include <Python.h>
#include <structmember.h>

#include "mcas_python_config.h"
#include "pool_type.h"

using namespace component;

namespace global
{
extern unsigned debug_level;
}

typedef struct {
  PyObject_HEAD
  component::IMCAS * _mcas;
  int                _port;
} Session;

static PyObject * open_pool(Session* self, PyObject *args, PyObject *kwds);
static PyObject * create_pool(Session* self, PyObject *args, PyObject *kwds);
static PyObject * delete_pool(Session* self, PyObject *args, PyObject *kwds);
static PyObject * get_stats(Session* self, PyObject *args, PyObject *kwds);

  
static PyObject *
Session_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  auto self = (Session *)type->tp_alloc(type, 0);
  assert(self);
  return (PyObject*)self;
}



/** 
 * tp_dealloc: Called when reference count is 0
 * 
 * @param self 
 */
static void
Session_dealloc(Session *self)
{
  assert(self);

  if(global::debug_level > 0)
    PLOG("Session: dealloc (%p)", self);
  
  if(self->_mcas)
    self->_mcas->release_ref();
  
  assert(self);
  Py_TYPE(self)->tp_free((PyObject*)self);

}


static int Session_init(Session *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"ip",
                                 "port",
                                 "device",
                                 "extra",
                                 "debug",
                                 NULL};

  const char * p_ip = nullptr;
  const char * p_device = nullptr;
  const char * p_ext = nullptr;
  int port = DEFAULT_PORT;
  int debug_level = 0;

  if (! PyArg_ParseTupleAndKeywords(args,
                                    kwds,
                                    "s|iszi",
                                    const_cast<char**>(kwlist),
                                    &p_ip,
                                    &port,
                                    &p_device,
                                    &p_ext,
                                    &debug_level)) {
    PyErr_SetString(PyExc_RuntimeError, "bad arguments");
    PWRN("bad arguments or argument types to Session constructor");
    return -1;
  }

  global::debug_level = debug_level;
  if(global::debug_level > 0)
    PLOG("Session: init");


  std::string device = p_device ? p_device : DEFAULT_DEVICE;

  std::stringstream addr;
  addr << p_ip << ":" << port;

  if(global::debug_level > 0)
    PLOG("Session: init (addr=%s, device=%s)", addr.str().c_str(), device.c_str());

  using namespace component;
  
  /* create object instance through factory */
  std::string path = MCAS_LIB_PATH;
  path += "/libcomponent-mcasclient.so";
  
  IBase *comp = load_component(path.c_str(), mcas_client_factory);

  if(comp == nullptr) {
    PyErr_SetString(PyExc_RuntimeError, "mcas.Session failed to load libcomponent-mcasclient.so");
    return -1;
  }
  
  auto fact = make_itf_ref(static_cast<IMCAS_factory *>(comp->query_interface(IMCAS_factory::iid())));
  if(fact == nullptr) {
    PyErr_SetString(PyExc_RuntimeError, "mcas.Session failed to get IMCAS_factory");
    return -1;
  }

  char * p_env_user_name = getenv("USER");
  std::string user_name;
  if(p_env_user_name) user_name = p_env_user_name;
  else user_name = "unknown";

  if(global::debug_level > 0)
    PLOG("Session: about to call mcas_create");

  try {
    if(p_ext) {
      self->_mcas = fact->mcas_create(debug_level,
                                      30, /* patience */
                                      user_name,
                                      addr.str(),
                                      device,
                                      p_ext);
    }
    else {
      self->_mcas = fact->mcas_create(debug_level,
                                      30, /* patience */
                                      user_name,
                                      addr.str(),
                                      device);
    }
  }
  catch(...) {
    if(global::debug_level > 0)
      PLOG("Session: fact->mcas_create failed (addr=%s, device=%s)", addr.str().c_str(), device.c_str());
  }
  
  if(self->_mcas == nullptr) {
    if(global::debug_level > 0)
      PLOG("Session: fact->mcas_create failed (addr=%s, device=%s)", addr.str().c_str(), device.c_str());

    PyErr_SetString(PyExc_RuntimeError, "mcas.Session failed to create session");
    return -1;
  }

  self->_port = port;

  if(global::debug_level > 0)
    PLOG("session: (%s)(%s) %p", addr.str().c_str(), device.c_str(), self->_mcas);
  return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"

static PyMemberDef Session_members[] = {
  {"port", T_ULONG, offsetof(Session, _port), READONLY, "Port"},
  {NULL}
};

#pragma GCC diagnostic pop

PyDoc_STRVAR(open_pool_doc,"Session.open_pool(name,[readonly=True]) -> Open pool.");
PyDoc_STRVAR(create_pool_doc,"Session.create_pool(name,pool_size,objcount) -> Create pool.");
PyDoc_STRVAR(delete_pool_doc,"Session.delete_pool(name) -> Delete pool.");
PyDoc_STRVAR(get_stats_doc,"Session.get_stats() -> Get shard statistics.");

static PyMethodDef Session_methods[] = {
  {"open_pool",  (PyCFunction) open_pool, METH_VARARGS | METH_KEYWORDS, open_pool_doc},
  {"create_pool",  (PyCFunction) create_pool, METH_VARARGS | METH_KEYWORDS, create_pool_doc},
  {"delete_pool",  (PyCFunction) delete_pool, METH_VARARGS | METH_KEYWORDS, delete_pool_doc},
  {"get_stats",  (PyCFunction) get_stats, METH_VARARGS | METH_KEYWORDS, get_stats_doc},
  {NULL}
};



PyTypeObject SessionType = {
  PyVarObject_HEAD_INIT(NULL, 0)
  "mcas.Session",           /* tp_name */
  sizeof(Session)   ,      /* tp_basicsize */
  0,                       /* tp_itemsize */
  (destructor) Session_dealloc,      /* tp_dealloc */
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
  "Session",              /* tp_doc */
  0,                       /* tp_traverse */
  0,                       /* tp_clear */
  0,                       /* tp_richcompare */
  0,                       /* tp_weaklistoffset */
  0,                       /* tp_iter */
  0,                       /* tp_iternext */
  Session_methods,         /* tp_methods */
  Session_members,         /* tp_members */
  0,                       /* tp_getset */
  0,                       /* tp_base */
  0,                       /* tp_dict */
  0,                       /* tp_descr_get */
  0,                       /* tp_descr_set */
  0,                       /* tp_dictoffset */
  (initproc)Session_init,  /* tp_init */
  0,            /* tp_alloc */
  Session_new,             /* tp_new */
  0, /* tp_free */
};


static PyObject * open_pool(Session* self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"name",
                                 "readonly",
                                 NULL};

  const char * pool_name = nullptr;
  int read_only = 0;
  
  if (! PyArg_ParseTupleAndKeywords(args,
                                    kwds,
                                    "s|p",
                                    const_cast<char**>(kwlist),
                                    &pool_name,
                                    &read_only)) {
    PyErr_SetString(PyExc_RuntimeError,"bad arguments");
    return NULL;
  }

  assert(self->_mcas);
  Pool * p = Pool_new();
  uint32_t flags = 0;
  if(read_only) flags |= component::IKVStore::FLAGS_READ_ONLY;

  self->_mcas->add_ref();

  assert(self->_mcas);
  p->_mcas = self->_mcas;
  p->_mcas->add_ref(); /* pool handle must hold reference to owner */
  p->_pool = self->_mcas->open_pool(pool_name, flags);  

  if(p->_pool == IKVStore::POOL_ERROR) {
    PyErr_SetString(PyExc_RuntimeError,"mcas.Session.open_pool failed");
    return NULL;
  }

  return (PyObject *) p;
}

static PyObject * create_pool(Session* self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"name",
                                 "size",
                                 "objcount",
                                 "create_only",
                                 NULL};

  const char * pool_name = nullptr;
  unsigned long size = 0;
  unsigned long objcount = 0;
  int create_only = 0;
  
  if (! PyArg_ParseTupleAndKeywords(args,
                                    kwds,
                                    "sk|kp",
                                    const_cast<char**>(kwlist),
                                    &pool_name,
                                    &size,
                                    &objcount,
                                    &create_only)) {
    PyErr_SetString(PyExc_RuntimeError,"bad arguments");
    return NULL;
  }

  assert(self->_mcas);
  Pool * p = Pool_new();
  
  uint32_t flags = 0;
  if(create_only) flags |= component::IKVStore::FLAGS_CREATE_ONLY;

  assert(self->_mcas);
  p->_mcas = self->_mcas;
  p->_mcas->add_ref(); /* pool handle must hold reference to owner */
  p->_pool = self->_mcas->create_pool(pool_name,
                                         size,
                                         flags,
                                         objcount);

  if(p->_pool == IKVStore::POOL_ERROR) {
    PyErr_SetString(PyExc_RuntimeError,"mcas.Session.create_pool failed");
    return NULL;
  }

  return (PyObject *) p;
}


static PyObject * delete_pool(Session* self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"name",
                                 NULL};

  const char * pool_name = nullptr;
  
  if (! PyArg_ParseTupleAndKeywords(args,
                                    kwds,
                                    "s",
                                    const_cast<char**>(kwlist),
                                    &pool_name)) {
    PyErr_SetString(PyExc_RuntimeError,"bad arguments");
    return NULL;
  }

  assert(self->_mcas);

  status_t hr = self->_mcas->delete_pool(pool_name);

  if(hr != S_OK) {
    std::stringstream ss;
    ss << "mcas.Session.create_pool failed [status:" << hr << "]";
    PyErr_SetString(PyExc_RuntimeError,ss.str().c_str());
    return NULL;
  }

  Py_RETURN_NONE;
}

#define macro_add_dict_item(X) PyDict_SetItemString(dict, #X, PyLong_FromUnsignedLongLong(stats.X));

static PyObject * get_stats(Session* self, PyObject *args, PyObject *kwds)
{
  component::IMCAS::Shard_stats stats;

  if(global::debug_level > 0)
    PLOG("mcas.Session.get_statistics ");
  
  status_t hr = self->_mcas->get_statistics(stats);

  if(hr != S_OK) {
    std::stringstream ss;
    ss << "mcas.Session.get_statistics failed [status:" << hr << "]";
    PyErr_SetString(PyExc_RuntimeError,ss.str().c_str());
    return NULL;
  }

  /* convert result to dictionary */
  PyObject* dict = PyDict_New();
  macro_add_dict_item(op_request_count);
  macro_add_dict_item(op_put_count);
  macro_add_dict_item(op_get_count);
  macro_add_dict_item(op_put_direct_count);
  macro_add_dict_item(op_get_twostage_count);
  macro_add_dict_item(op_erase_count);
  macro_add_dict_item(op_failed_request_count);
  macro_add_dict_item(last_op_count_snapshot);

  return dict;
}


