#ifndef __PYMM_LIST_TYPE_H__
#define __PYMM_LIST_TYPE_H__

#include <common/errors.h>
#include <common/logging.h>
#include <common/utils.h>

#include <ccpm/cca.h>
#include <ccpm/value_tracked.h>
#include <ccpm/container_cc.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Weffc++"
#include <EASTL/iterator.h>
#include <EASTL/list.h>
#pragma GCC diagnostic pop

#include <libpmem.h>
#include <Python.h>

typedef struct {
  PyObject_HEAD
  ccpm::cca * heap;
} List;

namespace
{
	struct pmem_persister final
		: public ccpm::persister
	{
		void persist(common::byte_span s) override
		{
			::pmem_persist(::base(s), ::size(s));
		}
	};
}

pmem_persister persister;

static PyObject *
ListType_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  auto self = (List *)type->tp_alloc(type, 0);
  assert(self);
  return (PyObject*)self;
}
/** 
 * tp_dealloc: Called when reference count is 0
 * 
 * @param self 
 */
static void
ListType_dealloc(List *self)
{
  delete self->heap;
  
  assert(self);
  Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject * ListType_method_append(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"element",
                                 "name",
                                 NULL};

  const char * element_name = nullptr;
  PyObject * element_object = nullptr;

  if (! PyArg_ParseTupleAndKeywords(args,
                                    kwds,
                                    "O|s",
                                    const_cast<char**>(kwlist),
                                    &element_object,
                                    &element_name)) {
     PyErr_SetString(PyExc_RuntimeError, "ListType_method_append unable to parse args");
     return NULL;
  }

  if(element_name) { /* add a reference to an already shelved item */
    PNOTICE("Append! shelved (%s)", element_name);
  }
  else {

    if(PyLong_Check(element_object)) {
      
    }
    else if(PyUnicode_Check(element_object)) {
    }
    else {
      PyErr_SetString(PyExc_RuntimeError, "ListType_method_append don't know how to append this type");
      return NULL;
    }

    PNOTICE("Append! new");
  }

  Py_RETURN_NONE;
}


static PyMethodDef ListType_methods[] = 
  {
   {"append", (PyCFunction) ListType_method_append, METH_VARARGS | METH_KEYWORDS, "append(a) -> append 'a' to list"},
   {NULL}  /* Sentinel */
  };

static int
ListType_init(List *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"buffer",
                                 "rehydrate",
                                 NULL};

  PyObject * memoryview_object = nullptr;
  int rehydrate = 0; /* zero for new construction */

  if (! PyArg_ParseTupleAndKeywords(args,
                                    kwds,
                                    "Op",
                                    const_cast<char**>(kwlist),
                                    &memoryview_object,
                                    &rehydrate)) {
     PyErr_SetString(PyExc_RuntimeError, "ListType ctor unable to parse args");
     return -1;
  }

  if (! PyMemoryView_Check(memoryview_object)) {
    PyErr_SetString(PyExc_RuntimeError, "ListType ctor parameter is not a memory view");
     return -1;
  }
  Py_buffer * buffer = PyMemoryView_GET_BUFFER(memoryview_object);
  assert(buffer);

  /* create or rehydrate the heap */
  ccpm::region_vector_t rv(ccpm::region_vector_t::value_type(common::make_byte_span(buffer->buf,buffer->len)));
  if(rehydrate) {
    PNOTICE("rehydrating...");
    self->heap = new ccpm::cca(&persister, rv, ccpm::accept_all);
  }
  else {
    PNOTICE("new ctor...");
    self->heap = new ccpm::cca(&persister, rv);
  }

  return 0;
}


PyTypeObject ListType = {
  PyVarObject_HEAD_INIT(NULL, 0)
  "pymm.pymmcore.List",           /* tp_name */
  sizeof(List)   ,      /* tp_basicsize */
  0,                       /* tp_itemsize */
  (destructor) ListType_dealloc,      /* tp_dealloc */
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
  "ListType",              /* tp_doc */
  0,                       /* tp_traverse */
  0,                       /* tp_clear */
  0,                       /* tp_richcompare */
  0,                       /* tp_weaklistoffset */
  0,                       /* tp_iter */
  0,                       /* tp_iternext */
  ListType_methods,        /* tp_methods */
  0, //ListType_members,         /* tp_members */
  0,                       /* tp_getset */
  0,                       /* tp_base */
  0,                       /* tp_dict */
  0,                       /* tp_descr_get */
  0,                       /* tp_descr_set */
  0,                       /* tp_dictoffset */
  (initproc)ListType_init,  /* tp_init */
  0,            /* tp_alloc */
  ListType_new,             /* tp_new */
  0, /* tp_free */
};



#endif
