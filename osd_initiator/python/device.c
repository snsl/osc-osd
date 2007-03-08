/*
 * Device interface.
 */
#include <fcntl.h>
#include <Python.h>
#include "osd_initiator/command.h"
#include "osd_initiator/device.h"
#include "pyosd.h"

/*
 * OSDDevice type.
 */
struct pyosd_device {
	PyObject_HEAD;
	int fd;
};

/*
 * Open an fd.
 */
static PyObject *pyosd_device_open(PyObject *self, PyObject *args)
{
	struct pyosd_device *device = (struct pyosd_device *) self;
	char *s;
	int fd;

	if (!PyArg_ParseTuple(args, "s:open", &s))
		return NULL;

	fd = open(s, O_RDWR);
	if (fd < 0)
		return PyErr_SetFromErrnoWithFilename(PyExc_OSError, s);

	device->fd = fd;

	return Py_BuildValue("");
}

/*
 * Close the fd.
 */
static PyObject *pyosd_device_close(PyObject *self, PyObject *args)
{
	struct pyosd_device *device = (struct pyosd_device *) self;
	int ret;

	if (!PyArg_ParseTuple(args, ":close"))
		return NULL;

	ret = close(device->fd);
	if (ret < 0)
		return PyErr_SetFromErrno(PyExc_OSError);

	return Py_BuildValue("");
}

/*
 * Submit the command to the fd.
 */
static PyObject *pyosd_device_submit_and_wait(PyObject *self, PyObject *args)
{
	struct pyosd_device *device = (struct pyosd_device *) self;
	PyObject *o;
	struct pyosd_command *py_command;
	struct osd_command *command;
	int ret;

	if (!PyArg_ParseTuple(args, "O:submit_and_wait", &o))
		return NULL;

	if (!PyObject_TypeCheck(o, &pyosd_command_type)) {
		PyErr_SetString(PyExc_TypeError, "expecting a pyosd_command");
		return NULL;
	}

	py_command = (struct pyosd_command *) o;
	command = &py_command->command;

	ret = osd_submit_and_wait(device->fd, command);
	py_command->complete = 1;
	if (ret)
		return PyErr_SetFromErrno(PyExc_OSError);
	return Py_BuildValue("");
}

/*
 * Instance methods and type.
 */
struct PyMethodDef pyosd_device_methods[] = {
	{ "open", pyosd_device_open, METH_VARARGS,
		"Open a device." },
	{ "close", pyosd_device_close, METH_VARARGS,
		"Close the device." },
	{ "submit_and_wait", pyosd_device_submit_and_wait, METH_VARARGS,
		"Submit a command and wait for it to complete." },
	{ NULL }
};

PyTypeObject pyosd_device_type = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = "OSDDevice",
	.tp_basicsize = sizeof(struct pyosd_device),
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_doc = "Python encapsulation of OSD device",
        .tp_new = PyType_GenericNew,
	.tp_methods = pyosd_device_methods,
};

