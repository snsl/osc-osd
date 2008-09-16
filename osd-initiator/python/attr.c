/*
 * Attribute bits.
 *
 * Copyright (C) 2007 OSD Team <pvfs-osd@osc.edu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <Python.h>
#include <structmember.h>  /* T_UBYTE */
#include "osd-initiator/command.h"
#include "osd-util/osd-util.h"
#include "pyosd.h"

/*
 * Constructor function.
 *    GET      page number       expected_len
 *    GETPAGE  page expected_len
 *    GETMULTI page number       expected_len_each
 *    SET      page number       val
 *    SET      page number       None  == delete
 */
static int pyosd_attr_init(PyObject *self, PyObject *args,
			   PyObject *keywords __unused)
{
	struct pyosd_attr *py_attr = (struct pyosd_attr *) self;
	struct attribute_list *attr = &py_attr->attr;
	int attr_type;
	unsigned int page, number = 0, len = 0;
	PyObject *val = NULL;

	if (!PyArg_ParseTuple(args, "iO|OO:init", &attr_type, &val, &val, &val))
		return -1;

	if (attr_type == ATTR_GET || attr_type == ATTR_GET_MULTI) {
		if (!PyArg_ParseTuple(args, "iIII:init", &attr_type, &page,
				      &number, &len))
			return -1;
	} else if (attr_type == ATTR_GET_PAGE) {
		if (!PyArg_ParseTuple(args, "iII:init", &attr_type, &page,
				      &len))
			return -1;
	} else if (attr_type == ATTR_SET) {
		if (!PyArg_ParseTuple(args, "iIIO:init", &attr_type, &page,
				      &number, &val))
			return -1;
	} else {
		PyErr_Format(PyExc_TypeError, "type %d unknown", attr_type);
		return -1;
	}

	attr->type = attr_type;
	attr->page = page;
	attr->number = number;
	if (attr_type == ATTR_SET) {
		/* memcpy now to avoid complexity with figuring out freeing */
		if (PyString_Check(val)) {
			attr->len = PyString_Size(val);
			attr->val = PyMem_Malloc(attr->len);
			memcpy(attr->val, PyString_AsString(val),
			       attr->len);
		} else if (PyInt_Check(val)) {
			uint32_t x;
			attr->len = 4;
			attr->val = PyMem_Malloc(4);
			x = PyInt_AsLong(val);
			memcpy(attr->val, &x, attr->len);
		} else if (PyLong_Check(val)) {
			uint64_t x;
			attr->len = 8;
			attr->val = PyMem_Malloc(8);
			x = PyLong_AsUnsignedLongLong(val);
			memcpy(attr->val, &x, attr->len);
		} else if (val == Py_None) {
			attr->len = 0;
			attr->val = NULL;  /* delete attribute */
		} else {
			PyErr_SetString(PyExc_RuntimeError,
					"cannot linearize this type");
			return -1;
		}
	} else {
		attr->val = NULL;
		attr->len = len;
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
 * Pretty-print object.
 */
static PyObject *pyosd_attr_print(PyObject *self)
{
	struct pyosd_attr *py_attr = (struct pyosd_attr *) self;
	const struct attribute_list *attr = &py_attr->attr;
	const char *type = NULL;
	PyObject *o;

	if (attr->type == ATTR_GET)
		type = "ATTR_GET";
	else if (attr->type == ATTR_GET_PAGE)
		type = "ATTR_GET_PAGE";
	else if (attr->type == ATTR_GET_MULTI)
		type = "ATTR_GET_MULTI";
	else if (attr->type == ATTR_SET)
		type = "ATTR_SET";
	o = PyString_FromFormat(
		"type %s page 0x%x number 0x%x len %d outlen %d",
				type, attr->page, attr->number, attr->len,
				attr->outlen);
	return o;
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
			Py_RETURN_NONE;
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
	.tp_str = pyosd_attr_print,
};

