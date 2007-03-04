/*
 * Python interface to all of osd_initiator.
 */
#include <stdio.h>
#include <fcntl.h>
#include <Python.h>
#include <structmember.h>  /* python header */
#include "util/util.h"
#include "osd_initiator/diskinfo.h"
#include "osd_initiator/kernel_interface.h"
#include "osd_initiator/user_interface_sgio.h"  /* struct attribute_id */
#include "osd_initiator/cdb_manip.h"
#include "osd_initiator/osd_cmds.h"
#include "osd_initiator/sense.h"

#define __unused __attribute__((unused))

/*
 * OSDDrive type.
 */
struct pyosd_drive {
	PyObject_HEAD;
	PyObject *targetname;
	PyObject *chardev;
};
PyTypeObject pyosd_drive_type;  /* forward decl */

/*
 * OSDDriveList type.
 */
struct pyosd_drivelist {
	PyListObject list;
};

/*
 * OSDDevice type.
 */
struct pyosd_device {
	PyObject_HEAD;
	int fd;
};

/*
 * OSDCommand type.
 */
struct pyosd_command {
	PyObject_HEAD;
	struct osd_command command;
	int set;
	int complete;
	struct attribute_id *attrs;
	int numattrs;
	PyObject *attrlist;
};
PyTypeObject pyosd_command_type;  /* forward decl */

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
 * Get the drive list.
 */
static int pyosd_drivelist_init(PyObject *self, PyObject *args,
				PyObject *keywords)
{
	int ret, i;
	struct osd_drive_description *drives;
	int num_drives;
	struct pyosd_drive *drive;

        ret = PyList_Type.tp_init(self, args, keywords);
	if (ret) {
		PyErr_SetString(PyExc_RuntimeError, "list init failed");
		return ret;
	}

	ret = osd_get_drive_list(&drives, &num_drives);
	if (ret) {
		PyErr_SetString(PyExc_RuntimeError, "get_drive_list failed");
		return -1;
	}

	for (i=0; i<num_drives; i++) {
		drive = PyObject_New(typeof(*drive), &pyosd_drive_type);
		drive->targetname = PyString_FromString(drives[i].targetname);
		drive->chardev = PyString_FromString(drives[i].chardev);
		Py_INCREF(drive);
		PyList_Append(self, (PyObject *) drive);
	}

	osd_free_drive_list(drives, num_drives);
	return 0;
}

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

static void pyosd_command_attr_resolve(struct pyosd_command *py_command);

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

	ret = osd_sgio_submit_and_wait(device->fd, command);
	py_command->complete = 1;
	if (ret)
		return PyErr_SetFromErrno(PyExc_OSError);
	pyosd_command_attr_resolve(py_command);
	return Py_BuildValue("");
}

/*
 * Clear everything.
 */
static int pyosd_command_init(PyObject *self, PyObject *args __unused,
			      PyObject *keywords __unused)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;

	memset(command, 0, sizeof(*command));
	py_command->set = 0;
	py_command->complete = 0;
	py_command->attrs = NULL;
	py_command->numattrs = 0;
	py_command->attrlist = NULL;
    	return 0;
}

static void pyosd_command_dealloc(PyObject *self)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;

	if (command->indata) {
	    	if (command->iov_inlen) {
		    	struct bsg_iovec *iov = command->indata;
			int i;
		    	for (i=0; i<command->iov_inlen; i++)
			    	PyMem_Free((void *) iov[i].iov_base);
		} else {
			if (py_command->numattrs)  /* hack */
				free(command->indata);
			else
				PyMem_Free(command->indata);
		}
	}
	if (command->outdata) {
	    	if (command->iov_outlen) {
		    	const struct bsg_iovec *iov = command->outdata;
			int i;
		    	for (i=0; i<command->iov_outlen; i++)
			    	PyMem_Free((void *) iov[i].iov_base);
		} else {
			if (py_command->numattrs)  /* hack */
				free((void *)(uintptr_t) command->outdata);
			else
				PyMem_Free((void *)(uintptr_t) command->outdata);
		}
	}
	if (py_command->attrs)
		PyMem_Free(py_command->attrs);
}

static PyObject *pyosd_command_get_indata(PyObject *self,
					  void *closure __unused)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;

	if (!py_command->complete) {
		PyErr_SetString(PyExc_RuntimeError, "command not complete");
		return NULL;
	}
	if (command->indata)
	    	return PyString_FromStringAndSize(command->indata,
		                                         command->inlen);
	return Py_BuildValue("");
}

/*
 * Return the returned attributes array.
 */
static PyObject *pyosd_command_get_attr(PyObject *self, void *closure __unused)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;

	if (!py_command->complete) {
		PyErr_SetString(PyExc_RuntimeError, "command not complete");
		return NULL;
	}
	if (!py_command->numattrs) {
		PyErr_SetString(PyExc_RuntimeError, "no attributes");
		return NULL;
	}

	Py_INCREF(py_command->attrlist);
	return py_command->attrlist;
}

static PyObject *pyosd_command_show_sense(PyObject *self, PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	char *s;
	PyObject *o;

	if (!PyArg_ParseTuple(args, ":show_sense"))
		return NULL;
	if (!py_command->complete) {
		PyErr_SetString(PyExc_RuntimeError, "command not complete");
		return NULL;
	}

	if (command->status == 0)
		return Py_BuildValue("s", "No sense");
	if (command->status != 2)
		return Py_BuildValue("s", "Unknown status code");
	s = osd_show_sense(command->sense, command->sense_len);
	o = Py_BuildValue("s", s);
	free(s);
	return o;
}

/*
 * Attributes.
 */
static PyObject *pyosd_command_attr_get(PyObject *self, PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	uint32_t page, number, result_len = 0;
	PyObject *result_objtype;

	/* XXX: accept a list of these things too... */
	if (!PyArg_ParseTuple(args, "IIO|I:set_create_partition", &page,
	                      &number, &result_objtype, &result_len))
		return NULL;
	if (!py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command not set");
		return NULL;
	}
	if (py_command->complete) {
		PyErr_SetString(PyExc_RuntimeError, "command already complete");
		return NULL;
	}
	if (py_command->numattrs) {
		PyErr_SetString(PyExc_RuntimeError, "attrs already set");
		return NULL;
	}

	if (!PyType_Check(result_objtype)) {
		PyErr_SetString(PyExc_RuntimeError, "need a type");
		return NULL;
	}

	if ((PyTypeObject *)result_objtype == &PyInt_Type)
		result_len = 4;
	else if ((PyTypeObject *)result_objtype == &PyLong_Type)
		result_len = 8;
	else if ((PyTypeObject *)result_objtype == &PyString_Type) {
		if (result_len == 0) {
			PyErr_SetString(PyExc_RuntimeError,
				"must supply length for string");
			return NULL;
	    	}
	} else {
		PyErr_SetString(PyExc_RuntimeError,
				"don't know length of this type");
		return NULL;
	}

	if (command->indata || command->outdata) {
		PyErr_SetString(PyExc_RuntimeError,
		                "don't know how to handle data too");
		return NULL;
	}

	py_command->numattrs = 1;
	py_command->attrs = PyMem_Malloc(py_command->numattrs
	                                 * sizeof(*py_command->attrs));
	if (py_command->attrs == NULL)
	    	return PyErr_NoMemory();

	py_command->attrs[0].page = page;
	py_command->attrs[0].number = number;
	py_command->attrs[0].len = result_len;
	py_command->attrs[0].tag = result_objtype;

	osd_command_attr_build(command, py_command->attrs,
			       py_command->numattrs);

	return Py_BuildValue("");
}

/*
 * If there were attributes associated with this command, build the
 * resulting objects that were returned from the server.  Called after
 * a device "wait" method returns.
 */
static void pyosd_command_attr_resolve(struct pyosd_command *py_command)
{
	struct osd_command *command = &py_command->command;
	PyObject *o;
	PyTypeObject *type;
	uint8_t *d;
	int i;

	if (py_command->numattrs == 0)
		return;

	py_command->attrlist = PyList_New(py_command->numattrs);
	for (i=0; i<py_command->numattrs; i++) {
		type = py_command->attrs[i].tag;
		d = osd_command_attr_resolve(command, py_command->attrs,
	                                     py_command->numattrs, i);
		if (type == &PyInt_Type)
			o = PyInt_FromLong(ntohl(d));
		else if (type == &PyLong_Type)
			o = PyLong_FromUnsignedLongLong(ntohll(d));
		else if (type == &PyString_Type)
			o = PyString_FromStringAndSize((char *) d,
						   py_command->attrs[i].len);
	    	PyList_SetItem(py_command->attrlist, i, o);
	}
}


/*
 * One of these for each of the major set functions.
 */
static PyObject *pyosd_command_set_inquiry(PyObject *self, PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;

	if (!PyArg_ParseTuple(args, ":inquiry"))
		return NULL;
	if (py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command already set");
		return NULL;
	}

	py_command->set = 1;
	command->cdb[0] = INQUIRY;
	command->cdb[4] = 80;
        command->cdb_len = 6;
	command->indata = PyMem_Malloc(80);
	if (command->indata == NULL)
	    	return PyErr_NoMemory();
	command->inlen_alloc = 80;
	return Py_BuildValue("");
}

static PyObject *pyosd_command_set_format_osd(PyObject *self, PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	uint64_t capacity;

	if (!PyArg_ParseTuple(args, "K:set_format_osd", &capacity))
		return NULL;
	if (py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command already set");
		return NULL;
	}

	py_command->set = 1;
        set_cdb_osd_format_osd(command->cdb, capacity);
        command->cdb_len = OSD_CDB_SIZE;
	return Py_BuildValue("");
}

static PyObject *pyosd_command_set_create(PyObject *self, PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	uint64_t pid = 0, oid = 0;

	if (!PyArg_ParseTuple(args, "K|K:set_create", &pid, &oid))
		return NULL;
	if (py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command already set");
		return NULL;
	}

	py_command->set = 1;
        set_cdb_osd_create(command->cdb, pid, oid, 1);
        command->cdb_len = OSD_CDB_SIZE;
	return Py_BuildValue("");
}

static PyObject *pyosd_command_set_create_partition(PyObject *self,
						    PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	uint64_t pid = 0;

	if (!PyArg_ParseTuple(args, "|K:set_create_partition", &pid))
		return NULL;
	if (py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command already set");
		return NULL;
	}

	py_command->set = 1;
        set_cdb_osd_create_partition(command->cdb, pid);
        command->cdb_len = OSD_CDB_SIZE;
	return Py_BuildValue("");
}

/*
 * OSDDrive and its two members but no methods.
 */
struct PyMemberDef pyosd_drive_members[] = {
	{ "targetname", T_OBJECT, offsetof(struct pyosd_drive, targetname),
		READONLY, "target name" },
	{ "chardev", T_OBJECT, offsetof(struct pyosd_drive, chardev),
		READONLY, "character device" },
	{ NULL }
};

PyTypeObject pyosd_drive_type = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = "OSDDrive",
	.tp_basicsize = sizeof(struct pyosd_drive),
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_doc = "Python encapsulation of OSD drive",
	.tp_members = pyosd_drive_members,
};

/*
 * OSDDriveList is just a list of OSDDrive.   No methods or members.
 */
PyTypeObject pyosd_drivelist_type = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = "OSDDriveList",
	.tp_basicsize = sizeof(struct pyosd_drivelist),
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_doc = "Python encapsulation of OSD drive list",
	.tp_init = pyosd_drivelist_init,
	.tp_base = &PyList_Type,
};

/*
 * OSDDevice instance methods and type.
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

/*
 * OSDCommand instance methods, members, and type.
 */
struct PyMethodDef pyosd_command_methods[] = {
	{ "show_sense", pyosd_command_show_sense, METH_VARARGS,
		"Generate a string of returned sense data." },
	{ "set_inquiry", pyosd_command_set_inquiry, METH_VARARGS,
		"Build the INQURY command." },
	{ "set_format_osd", pyosd_command_set_format_osd, METH_VARARGS,
		"Build the FORMAT_OSD command." },
	{ "set_create", pyosd_command_set_create,
		METH_VARARGS, "Build the CREATE command." },
	{ "set_create_partition", pyosd_command_set_create_partition,
		METH_VARARGS, "Build the CREATE_PARTITION command." },
	{ "attr_get", pyosd_command_attr_get, METH_VARARGS,
		"Modify a command to get a particular attribute." },
	{ NULL }
};

struct PyMemberDef pyosd_command_members[] = {
	{ "status", T_UBYTE, offsetof(struct pyosd_command, command.status),
		READONLY, "command status after completion" },
	{ "inlen", T_ULONG, offsetof(struct pyosd_command, command.inlen),
		READONLY, "returned data length" },
	{ NULL }
};

/* more complex members with their own functions */
struct PyGetSetDef pyosd_command_getset[] = {
    	{ "indata", pyosd_command_get_indata, NULL, "returned data", NULL },
    	{ "attr", pyosd_command_get_attr, NULL, "returned data", NULL },
};

PyTypeObject pyosd_command_type = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = "OSDCommand",
	.tp_basicsize = sizeof(struct pyosd_command),
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_doc = "Python encapsulation of struct osd_command",
	.tp_init = pyosd_command_init,
        .tp_new = PyType_GenericNew,
	.tp_dealloc = pyosd_command_dealloc,
	.tp_methods = pyosd_command_methods,
	.tp_members = pyosd_command_members,
	.tp_getset = pyosd_command_getset,
};

/*
 * Class (not instance) method.
 */
static PyMethodDef methods[] = {
	{ "set_progname", pyosd_set_progname, METH_VARARGS,
		"Set the program name for error reporting." },
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


/* picked up by the python module loader; this avoids a compilation warning */
PyMODINIT_FUNC initpyosd(void);

PyMODINIT_FUNC initpyosd(void)
{
	int ret = 0;
	PyObject *m;

	m = Py_InitModule("pyosd", methods);
	if (m == NULL) {
		fprintf(stderr, "pyosd: Py_InitModule failed\n");
		return;
	}

	ret += ready_type(m, &pyosd_command_type);
	ret += ready_type(m, &pyosd_device_type);
	ret += ready_type(m, &pyosd_drive_type);
	ret += ready_type(m, &pyosd_drivelist_type);
	if (ret)
		fprintf(stderr, "pyosd: ready_type failed\n");

	/* PyEval_SetTrace(pyosd_trace, NULL); */
}

