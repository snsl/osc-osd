/*
 * Lines that look like #include will be processed with extract.py to produce struct
 * and function declarations to be included in the python interface.
 */
#include <stddef.h>
#include <stdint.h>

cdef extern from "Python.h":
	void Py_XINCREF (object)
	void Py_XDECREF (object)
	object PyString_FromStringAndSize(char *, int)
	ctypedef void *PyGILState_STATE
	void PyErr_Clear()
	PyGILState_STATE PyGILState_Ensure()
	void PyGILState_Release(PyGILState_STATE)

#include "../../kernel/suo_ioctl.h"
#include "../cdb_manip.h"
#include "../kernel_interface.h"
#include "../user_interface_sgio.h"
#include "../diskinfo.h"

