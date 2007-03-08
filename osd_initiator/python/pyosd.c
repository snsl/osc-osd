/*
 * Main module.
 */
#include <Python.h>
#include "osd_initiator/command.h"
#include "util/util.h"
#include "pyosd.h"

/*
 * Initialize util.h error reporting.
 */
static PyObject *pyosd_set_progname(PyObject *self __unused, PyObject *args)
{
	char *argv[1];

	if (!PyArg_ParseTuple(args, "s:set_progname", &argv[0]))
		return NULL;

	osd_set_progname(1, argv);

	return Py_BuildValue("");
}

/*
 * Byte swappers.
 */
static PyObject *pyosd_ntohs(PyObject *self __unused, PyObject *args)
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
	return Py_BuildValue("H", ntohs(s));
}

static PyObject *pyosd_ntohl(PyObject *self __unused, PyObject *args)
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
	return Py_BuildValue("I", ntohl(s));
}

static PyObject *pyosd_ntohll(PyObject *self __unused, PyObject *args)
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
	return Py_BuildValue("K", ntohll(s));
}

/*
 * Class (not instance) methods.
 */
static PyMethodDef methods[] = {
	{ "set_progname", pyosd_set_progname, METH_VARARGS,
		"Set the program name for error reporting." },
	{ "ntohs", pyosd_ntohs, METH_VARARGS,
		"Convert 16-bit word from network to host byte order." },
	{ "ntohl", pyosd_ntohl, METH_VARARGS,
		"Convert 32-bit word from network to host byte order." },
	{ "ntohll", pyosd_ntohll, METH_VARARGS,
		"Convert 64-bit word from network to host byte order." },
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

/*
 * Insert strings into the module dictionary.
 */
static int dict_insert(PyObject *d, const char *name, int val)
{
        PyObject *v;
	
	v = PyInt_FromLong(val);
	if (!v)
		return 1;

        if (PyDict_SetItemString(d, name, v) < 0)
                return 1;

        Py_DECREF(v);
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
	ret += dict_insert(d, "ATTR_GET", ATTR_GET);
	ret += dict_insert(d, "ATTR_GET_PAGE", ATTR_GET_PAGE);
	ret += dict_insert(d, "ATTR_GET_MULTI", ATTR_GET_MULTI);
	ret += dict_insert(d, "ATTR_SET", ATTR_SET);

	ret += ready_type(m, &pyosd_command_type);
	ret += ready_type(m, &pyosd_attr_type);
	ret += ready_type(m, &pyosd_device_type);
	ret += ready_type(m, &pyosd_drive_type);
	ret += ready_type(m, &pyosd_drivelist_type);
	if (ret)
		fprintf(stderr, "pyosd: ready_type failed\n");

	/* PyEval_SetTrace(pyosd_trace, NULL); */
}

