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
	int attr_type;
	unsigned int page, number, len;

	if (!PyArg_ParseTuple(args, "iIII:submit_and_wait", &attr_type, &page,
			      &number, &len))
		return -1;
	py_attr->attr.type = attr_type;
	py_attr->attr.page = page;
	py_attr->attr.number = number;
	py_attr->attr.len = len;
    	return 0;
}

/*
 * Destructor.  Free val members of attributes.
 */
static void pyosd_attr_dealloc(PyObject *self)
{
	struct pyosd_attr *py_attr = (struct pyosd_attr *) self;
	struct attribute_list *attr = &py_attr->attr;
	int i, numattr = py_attr->numattr;

	for (i=0; i<numattr; i++)
		if (attr[i].type == ATTR_GET || attr[i].type == ATTR_GET_PAGE
		 || attr[i].type == ATTR_GET_MULTI)
			PyMem_Free(attr[i].val);
}

/*
 * Return val.
 */
static PyObject *pyosd_attr_get_val(PyObject *self, void *closure __unused)
{
	struct pyosd_attr *py_attr = (struct pyosd_attr *) self;
	const struct attribute_list *attr = &py_attr->attr;

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

