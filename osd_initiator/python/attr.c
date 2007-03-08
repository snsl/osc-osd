/*
 * Attribute bits.
 */
#include <Python.h>
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
	unsigned int page, number, len;
	PyObject *val = NULL;

	if (!PyArg_ParseTuple(args, "iIII|S:init", &attr_type, &page,
			      &number, &len, &val))
		return -1;
	attr->type = attr_type;
	attr->page = page;
	attr->number = number;
	attr->len = len;
	if (val) {
		if (attr_type != ATTR_SET) {
		    PyErr_SetString(PyExc_RuntimeError,
		    		    "value can only be given for ATTR_SET");
		    return -1;
		}
		Py_IncRef(val);
		py_attr->val = val;
		attr->val = PyString_AsString(val);  /* pointer into val */
	} else {
		if (attr_type == ATTR_SET) {
			PyErr_SetString(PyExc_RuntimeError,
					"value must be given for ATTR_SET");
			return -1;
		}
		attr->val = PyMem_Malloc(attr->len);  /* for output data */
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

	if (attr->type == ATTR_GET || attr->type == ATTR_GET_PAGE
	 || attr->type == ATTR_GET_MULTI)
		PyMem_Free(attr->val);    /* output data */

	if (attr->type == ATTR_SET)
		Py_DecRef(py_attr->val);  /* ref on input data */
}

/*
 * Return val.  Should keep a ref to the command to check if it is done.
 */
static PyObject *pyosd_attr_get_val(PyObject *self, void *closure __unused)
{
	struct pyosd_attr *py_attr = (struct pyosd_attr *) self;
	const struct attribute_list *attr = &py_attr->attr;

	if (!(attr->type == ATTR_GET || attr->type == ATTR_GET_PAGE
	   || attr->type == ATTR_GET_MULTI)) {
		PyErr_SetString(PyExc_RuntimeError, 
				"only GET values can be retrieved");
	}
	if (attr->outlen == 0 || attr->outlen == 0xffff)
		return Py_BuildValue("");

	return Py_BuildValue("s#", attr->val, attr->outlen);
}

struct PyGetSetDef pyosd_attr_getset[] = {
    	{ "val", pyosd_attr_get_val, NULL, "returned attribute value", NULL },
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
};

