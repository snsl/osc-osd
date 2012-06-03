/*
 * Main module.
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
#include "osd-initiator/command.h"
#include "osd-util/osd-util.h"
#include "pyosd.h"

/*
 * Initialize util.h error reporting.
 */
static PyObject *pyosd_set_progname(PyObject *self __attribute__((unused)),
				    PyObject *args)
{
	char *argv[1];

	if (!PyArg_ParseTuple(args, "s:set_progname", &argv[0]))
		return NULL;

	osd_set_progname(1, argv);

	Py_RETURN_NONE;
}

/*
 * Retrieve the progname.
 */
static PyObject *pyosd_get_progname(PyObject *self __attribute__((unused)),
				    PyObject *args)
{
	char *argv[1];

	if (!PyArg_ParseTuple(args, ":get_progname", &argv[0]))
		return NULL;

	return Py_BuildValue("s", osd_get_progname());
}

/*
 * Byte swappers.
 */
static PyObject *pyosd_ntohs(PyObject *self __attribute__((unused)),
			     PyObject *args)
{
	const uint8_t *s;
	int len;

	if (!PyArg_ParseTuple(args, "s#:ntohs", &s, &len))
		return NULL;
	if (len != 2) {
		char s[1024];
		sprintf(s, "string len must be 2, was %d", len);
		PyErr_SetString(PyExc_RuntimeError, s);
		return NULL;
	}
	return Py_BuildValue("H", get_ntohs(s));
}

static PyObject *pyosd_ntohl(PyObject *self __attribute__((unused)),
			     PyObject *args)
{
	const uint8_t *s;
	int len;

	if (!PyArg_ParseTuple(args, "s#:ntohl", &s, &len))
		return NULL;
	if (len != 4) {
		char s[1024];
		sprintf(s, "string len must be 4, was %d", len);
		PyErr_SetString(PyExc_RuntimeError, s);
		return NULL;
	}
	return Py_BuildValue("I", get_ntohl(s));
}

static PyObject *pyosd_ntohll(PyObject *self __attribute__((unused)),
			      PyObject *args)
{
	const uint8_t *s;
	int len;

	if (!PyArg_ParseTuple(args, "s#:ntohll", &s, &len))
		return NULL;
	if (len != 8) {
		char s[1024];
		sprintf(s, "string len must be 8, was %d", len);
		PyErr_SetString(PyExc_RuntimeError, s);
		return NULL;
	}
	return Py_BuildValue("K", get_ntohll(s));
}

static PyObject *pyosd_htonl(PyObject *self __attribute__((unused)),
			     PyObject *args)
{
	uint32_t w;
	union {
	    uint8_t s[4];
	    uint32_t x;
	} u;

	if (!PyArg_ParseTuple(args, "I:htonl", &w))
		return NULL;
	set_htonl(u.s, w);
	return Py_BuildValue("I", u.x);
}

static PyObject *pyosd_htonll(PyObject *self __attribute__((unused)),
			      PyObject *args)
{
	uint64_t w;
	union {
	    uint8_t s[4];
	    uint64_t x;
	} u;

	if (!PyArg_ParseTuple(args, "K:htonll", &w))
		return NULL;
	set_htonll(u.s, w);
	return Py_BuildValue("K", u.x);
}

/*
 * Class (not instance) methods.
 */
static PyMethodDef methods[] = {
	{ "set_progname", pyosd_set_progname, METH_VARARGS,
		"Set the program name for error reporting." },
	{ "get_progname", pyosd_get_progname, METH_VARARGS,
		"Get the program name." },
	{ "ntohs", pyosd_ntohs, METH_VARARGS,
		"Convert 16-bit word from network to host byte order." },
	{ "ntohl", pyosd_ntohl, METH_VARARGS,
		"Convert 32-bit word from network to host byte order." },
	{ "ntohll", pyosd_ntohll, METH_VARARGS,
		"Convert 64-bit word from network to host byte order." },
	{ "htonl", pyosd_htonl, METH_VARARGS,
		"Convert 32-bit word from host to network byte order." },
	{ "htonll", pyosd_htonll, METH_VARARGS,
		"Convert 64-bit word from host to network byte order." },
	{ NULL }
};

/*
 * Helper function to build the types.
 */
static int ready_type(PyObject *module, PyTypeObject *type)
{
	if (PyType_Ready(type) < 0) {
		fprintf(stderr, "pyosd: PyType_Ready OSDCommand failed\n");
		return 1;
	}
	Py_INCREF(type);
	PyModule_AddObject(module, type->tp_name, (PyObject *) type);
	return 0;
}

#if 0
/*
 * Tracer function, for debugging, not very useful.
 */
static int pyosd_trace(PyObject *self, struct _frame *frame, int what,
                       PyObject *arg)
{
	if (what == PyTrace_LINE)
		printf("%s: line\n", __func__);
	else if (what == PyTrace_RETURN)
		printf("%s: return %p\n", __func__, arg);
	else
		printf("%s: what %d\n", __func__, what);
	return 0;
}
#endif

PyMODINIT_FUNC initpyosd(void)
{
	int ret = 0;
	PyObject *m;
	PyObject *d;

	m = Py_InitModule("pyosd", methods);
	if (m == NULL) {
		fprintf(stderr, "pyosd: Py_InitModule failed\n");
		return;
	}

	d = PyModule_GetDict(m);
	add_consts(d);

	ret += ready_type(m, &pyosd_command_type);
	ret += ready_type(m, &pyosd_attr_type);
	ret += ready_type(m, &pyosd_device_type);
	ret += ready_type(m, &pyosd_drive_type);
	ret += ready_type(m, &pyosd_drivelist_type);
	if (ret)
		fprintf(stderr, "pyosd: ready_type failed\n");

	/* PyEval_SetTrace(pyosd_trace, NULL); */
}

