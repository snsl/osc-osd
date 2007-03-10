/*
 * Attribute bits.
 */
#include <Python.h>
#include <structmember.h>  /* T_UBYTE */
#include "osd_initiator/command.h"
#include "util/util.h"
#include "pyosd.h"

/*
 * Constructor function.
 */
static int pyosd_attr_init(PyObject *self, PyObject *args,
			   PyObject *keywords __unused)
{
	struct pyosd_attr *py_attr = (struct pyosd_attr *) self;
	struct attribute_list *attr = &py_attr->attr;
	int attr_type;
	unsigned int page, number;
	PyObject *val_or_len = NULL;

	if (!PyArg_ParseTuple(args, "iIIO:init", &attr_type, &page,
			      &number, &val_or_len))
		return -1;
	attr->type = attr_type;
	attr->page = page;
	attr->number = number;
	if (attr_type == ATTR_SET) {
		/* memcpy now to avoid complexity with figuring out freeing */
		if (PyString_Check(val_or_len)) {
			attr->len = PyString_Size(val_or_len);
			attr->val = PyMem_Malloc(attr->len);
			memcpy(attr->val, PyString_AsString(val_or_len),
			       attr->len);
		} else if (PyInt_Check(val_or_len)) {
			uint32_t x;
			attr->len = 4;
			attr->val = PyMem_Malloc(4);
			x = PyInt_AsLong(val_or_len);
			memcpy(attr->val, &x, attr->len);
		} else if (PyLong_Check(val_or_len)) {
			uint64_t x;
			attr->len = 8;
			attr->val = PyMem_Malloc(8);
			x = PyLong_AsUnsignedLongLong(val_or_len);
			memcpy(attr->val, &x, attr->len);
		} else {
			PyErr_SetString(PyExc_RuntimeError,
					"cannot linearize this type");
			return -1;
		}
	} else {
		if (!PyInt_Check(val_or_len)) {
			PyErr_SetString(PyExc_RuntimeError,
					"must provide expected length");
			return -1;
		}
		attr->len = PyInt_AsLong(val_or_len);
		attr->val = NULL;
	}
    	return 0;
}

/*
 * Destructor.  Free val member if it was created for a get, or drop
 * its ref if it was passed in for a set.
 */
static void pyosd_attr_dealloc(PyObject *self)
{
	struct pyosd_attr *py_attr = (struct pyosd_attr *) self;
	struct attribute_list *attr = &py_attr->attr;

	/*
	 * If set, data was copied in constructor; if get, was copied
	 * as part of resolve.
	 */
	PyMem_Free(attr->val);
}

/*
 * Return val.  Should keep a ref to the command to check if it is done.
 */
static PyObject *pyosd_attr_get_val(PyObject *self, void *closure __unused)
{
	struct pyosd_attr *py_attr = (struct pyosd_attr *) self;
	const struct attribute_list *attr = &py_attr->attr;

	if (attr->type == ATTR_GET_MULTI) {
		PyErr_SetString(PyExc_RuntimeError, "handle this case.");
		return NULL;
	} else if (attr->type == ATTR_GET || attr->type == ATTR_GET_PAGE) {
		/* return None for 0xffff, but "" for 0 */
		if (attr->outlen < 0xffff)
			return Py_BuildValue("s#", attr->val, attr->outlen);
		else
			return Py_BuildValue("");
	} else {
		PyErr_SetString(PyExc_RuntimeError, 
				"only GET values can be retrieved");
		return NULL;
	}
}

struct PyGetSetDef pyosd_attr_getset[] = {
    	{ "val", pyosd_attr_get_val, NULL, "returned attribute value", NULL },
	{ NULL }
};

struct PyMemberDef pyosd_attr_members[] = {
	{ "outlen", T_USHORT, offsetof(struct pyosd_attr, attr.outlen),
		READONLY, "returned attribute length" },
	{ NULL }
};

PyTypeObject pyosd_attr_type = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = "OSDAttr",
	.tp_basicsize = sizeof(struct pyosd_attr),
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_doc = "Python encapsulation of struct attribute_list",
	.tp_dealloc = pyosd_attr_dealloc,
	.tp_init = pyosd_attr_init,
        .tp_new = PyType_GenericNew,
	.tp_getset = pyosd_attr_getset,
	.tp_members = pyosd_attr_members,
};

