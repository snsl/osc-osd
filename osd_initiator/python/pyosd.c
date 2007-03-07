/*
 * Python interface to all of osd_initiator.
 */
#include <stdio.h>
#include <fcntl.h>
#include <Python.h>
#include <structmember.h>  /* python header */
#include "util/util.h"
#include "osd_initiator/drivelist.h"
#include "osd_initiator/device.h"
#include "osd_initiator/command.h"
#include "osd_initiator/osd_cmds.h"
#include "osd_initiator/sense.h"

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
 * OSDAttr type.  Wrapper around attribute_list.  How to export enums
 * like pyosd.ATTR_GET?
 */
struct pyosd_attr {
	PyObject_HEAD;
	struct attribute_list attr;
	int numattr;
};
PyTypeObject pyosd_attr_type;  /* forward decl */

/*
 * OSDCommand type.
 */
struct pyosd_command {
	PyObject_HEAD;
	struct osd_command command;
	int set;
	int complete;
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
 * Byte swappers.
 */
static PyObject *pyosd_ntohs(PyObject *self __unused, PyObject *args)
{
	const uint8_t *s;
	int len;

	if (!PyArg_ParseTuple(args, "s#:ntohs", &s, &len))
		return NULL;
	if (len != 2) {
		PyErr_SetString(PyExc_RuntimeError, "string must be len 2");
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
		PyErr_SetString(PyExc_RuntimeError, "string must be len 4");
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
		PyErr_SetString(PyExc_RuntimeError, "string must be len 8");
		return NULL;
	}
	return Py_BuildValue("K", ntohll(s));
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
 * OSDAttr: clear everything.
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
 * OSDCommand: clear everything.
 */
static int pyosd_command_init(PyObject *self, PyObject *args __unused,
			      PyObject *keywords __unused)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;

	py_command->set = 0;
	py_command->complete = 0;
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
		} else
			PyMem_Free(command->indata);
	}
	if (command->outdata) {
	    	if (command->iov_outlen) {
		    	const struct bsg_iovec *iov = command->outdata;
			int i;
		    	for (i=0; i<command->iov_outlen; i++)
			    	PyMem_Free((void *) iov[i].iov_base);
		} else
			PyMem_Free((void *)(uintptr_t) command->outdata);
	}
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

static struct attribute_list *attr_flatten(PyObject *o, int *numattr)
{
	struct pyosd_attr *py_attr;
	struct attribute_list *attr = NULL;
	*numattr = 0;

	if (PyObject_TypeCheck(o, &pyosd_attr_type)) {
		py_attr = (struct pyosd_attr *) o;
		attr = PyMem_Malloc(sizeof(*attr));
		if (attr == NULL) {
			PyErr_NoMemory();
			return attr;
		}
		memcpy(attr, &py_attr->attr, sizeof(*attr));
		*numattr = 1;
	}

	if (PyList_Check(o)) {
		int i, num = PyList_Size(o);
		if (num < 1)
			goto out;

		attr = PyMem_Malloc(num * sizeof(*attr));
		if (attr == NULL) {
			PyErr_NoMemory();
			return NULL;
		}
		for (i=0; i<num; i++) {
			py_attr = (struct pyosd_attr *) PyList_GetItem(o, i);
			if (!PyObject_TypeCheck(py_attr, &pyosd_attr_type)) {
				PyMem_Free(attr);
				goto out;
			}
			memcpy(&attr[i], &py_attr->attr, sizeof(*attr));
		}
		*numattr = num;
	}

out:
	if (*numattr == 0) {
		PyErr_SetString(PyExc_RuntimeError,
				"need an OSDAttr or list of OSDAttr");
		return NULL;
	}
	return attr;
}

/*
 * Move the allocated and filled value buffers into the OSDAttr types.
 * The memory is managed by python and will be freed in the dealloc
 * function.  Object type has already been verified by the flatten function.
 */
static void attr_copy_vals(PyObject *o, struct attribute_list *attr,
			   int numattr)
{
	struct pyosd_attr *py_attr;

	if (PyObject_TypeCheck(o, &pyosd_attr_type)) {
		py_attr = (struct pyosd_attr *) o;
		py_attr->attr.val = attr[0].val;
		py_attr->attr.outlen = attr[0].outlen;
	}

	if (PyList_Check(o)) {
		int i;
		for (i=0; i<numattr; i++) {
			py_attr = (struct pyosd_attr *) PyList_GetItem(o, i);
			py_attr->attr.val = attr[i].val;
			py_attr->attr.outlen = attr[i].outlen;
		}
	}
}

/*
 * Attributes.  Valid argument to this function is OSDAttr or flat
 * list of OSDAttr.
 */
static PyObject *pyosd_command_attr_build(PyObject *self, PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	PyObject *o;
	struct attribute_list *attr;
	int ret, numattr;

	if (!PyArg_ParseTuple(args, "O:attr_build", &o))
		return NULL;

	if (!py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command not set");
		return NULL;
	}
	if (py_command->complete) {
		PyErr_SetString(PyExc_RuntimeError, "command already complete");
		return NULL;
	}

	/*
	 * Pull attrs into a single flat allocated list.
	 */
	attr = attr_flatten(o, &numattr);
	if (!attr)
		return NULL;

	ret = osd_command_attr_build(command, attr, numattr);
	if (ret) {
		PyErr_SetString(PyExc_RuntimeError, "attr_build failed");
		return NULL;
	}

	PyMem_Free(attr);

	return Py_BuildValue("");
}

/*
 * Must be called after the command runs, and only if attr_build was
 * used.  Could hide this in the run, but want to keep the python and
 * C APIs similar.
 *
 * We could also keep the attr with the command so there is no need
 * to pass it in again at resolve time.
 */
static PyObject *pyosd_command_attr_resolve(PyObject *self, PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	PyObject *o;
	struct attribute_list *attr;
	int i, ret, numattr;

	if (!PyArg_ParseTuple(args, "O:attr_resolve", &o))
		return NULL;

	if (!py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command not set");
		return NULL;
	}
	if (!py_command->complete) {
		PyErr_SetString(PyExc_RuntimeError, "command not complete");
		return NULL;
	}

	/*
	 * Pull attrs into a single flat allocated list.
	 */
	attr = attr_flatten(o, &numattr);
	if (!attr)
		return NULL;

	/*
	 * Allocate space for returned values.
	 */
	for (i=0; i<numattr; i++)
		if (attr[i].type == ATTR_GET || attr[i].type == ATTR_GET_PAGE
		 || attr[i].type == ATTR_GET_MULTI) {
			attr[i].val = PyMem_Malloc(attr[i].len);
			if (!attr[i].val)
				return PyErr_NoMemory();
		}

	ret = osd_command_attr_resolve(command, attr, numattr);
	if (ret) {
		PyErr_SetString(PyExc_RuntimeError, "attr_resolve failed");
		return NULL;
	}
	
	/*
	 * Copy results back out into py_attr.
	 */
	attr_copy_vals(o, attr, numattr);
	PyMem_Free(attr);

	return Py_BuildValue("");
}

/*
 * Return OSDAttr.val.
 */
static PyObject *pyosd_attr_get_val(PyObject *self, void *closure __unused)
{
	struct pyosd_attr *py_attr = (struct pyosd_attr *) self;
	const struct attribute_list *attr = &py_attr->attr;

	if (attr->outlen == 0 || attr->outlen == 0xffff)
		return Py_BuildValue("");

	return Py_BuildValue("s#", attr->val, attr->outlen);
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
	osd_command_set_inquiry(command, 80);
	command->indata = PyMem_Malloc(80);
	if (command->indata == NULL)
	    	return PyErr_NoMemory();
	command->inlen_alloc = 80;
	return Py_BuildValue("");
}

static PyObject *pyosd_command_set_append(PyObject *self, PyObject *args)
{
	PyErr_SetString(PyExc_RuntimeError, "unimplemented");
	return NULL;
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
        osd_command_set_create(command, pid, oid, 1);
	return Py_BuildValue("");
}

static PyObject *pyosd_command_set_create_and_write(PyObject *self,
						    PyObject *args)
{
	PyErr_SetString(PyExc_RuntimeError, "unimplemented");
	return NULL;
}

static PyObject *pyosd_command_set_create_collection(PyObject *self,
						     PyObject *args)
{
	PyErr_SetString(PyExc_RuntimeError, "unimplemented");
	return NULL;
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
        osd_command_set_create_partition(command, pid);
	return Py_BuildValue("");
}

static PyObject *pyosd_command_set_flush(PyObject *self, PyObject *args)
{
	PyErr_SetString(PyExc_RuntimeError, "unimplemented");
	return NULL;
}

static PyObject *pyosd_command_set_flush_collection(PyObject *self,
						    PyObject *args)
{
	PyErr_SetString(PyExc_RuntimeError, "unimplemented");
	return NULL;
}

static PyObject *pyosd_command_set_flush_osd(PyObject *self, PyObject *args)
{
	PyErr_SetString(PyExc_RuntimeError, "unimplemented");
	return NULL;
}

static PyObject *pyosd_command_set_flush_partition(PyObject *self,
						    PyObject *args)
{
	PyErr_SetString(PyExc_RuntimeError, "unimplemented");
	return NULL;
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
        osd_command_set_format_osd(command, capacity);
	return Py_BuildValue("");
}

static PyObject *pyosd_command_set_get_attributes(PyObject *self,
						    PyObject *args)
{
	PyErr_SetString(PyExc_RuntimeError, "unimplemented");
	return NULL;
}

static PyObject *pyosd_command_set_get_member_attributes(PyObject *self,
						    PyObject *args)
{
	PyErr_SetString(PyExc_RuntimeError, "unimplemented");
	return NULL;
}

static PyObject *pyosd_command_set_list(PyObject *self, PyObject *args)
{
	PyErr_SetString(PyExc_RuntimeError, "unimplemented");
	return NULL;
}

static PyObject *pyosd_command_set_list_collection(PyObject *self,
						   PyObject *args)
{
	PyErr_SetString(PyExc_RuntimeError, "unimplemented");
	return NULL;
}

static PyObject *pyosd_command_set_perform_scsi_command(PyObject *self,
						   PyObject *args)
{
	PyErr_SetString(PyExc_RuntimeError, "unimplemented");
	return NULL;
}

static PyObject *pyosd_command_set_perform_task_mgmt_func(PyObject *self,
						   PyObject *args)
{
	PyErr_SetString(PyExc_RuntimeError, "unimplemented");
	return NULL;
}

static PyObject *pyosd_command_set_query(PyObject *self, PyObject *args)
{
	PyErr_SetString(PyExc_RuntimeError, "unimplemented");
	return NULL;
}

static PyObject *pyosd_command_set_read(PyObject *self, PyObject *args)
{
	PyErr_SetString(PyExc_RuntimeError, "unimplemented");
	return NULL;
}

static PyObject *pyosd_command_set_remove(PyObject *self, PyObject *args)
{
	PyErr_SetString(PyExc_RuntimeError, "unimplemented");
	return NULL;
}

static PyObject *pyosd_command_set_remove_collection(PyObject *self,
						     PyObject *args)
{
	PyErr_SetString(PyExc_RuntimeError, "unimplemented");
	return NULL;
}

static PyObject *pyosd_command_set_remove_member_objects(PyObject *self,
						     PyObject *args)
{
	PyErr_SetString(PyExc_RuntimeError, "unimplemented");
	return NULL;
}

static PyObject *pyosd_command_set_remove_partition(PyObject *self,
						     PyObject *args)
{
	PyErr_SetString(PyExc_RuntimeError, "unimplemented");
	return NULL;
}

static PyObject *pyosd_command_set_set_attributes(PyObject *self,
						     PyObject *args)
{
	PyErr_SetString(PyExc_RuntimeError, "unimplemented");
	return NULL;
}

static PyObject *pyosd_command_set_set_key(PyObject *self,
						     PyObject *args)
{
	PyErr_SetString(PyExc_RuntimeError, "unimplemented");
	return NULL;
}

static PyObject *pyosd_command_set_set_master_key(PyObject *self,
						     PyObject *args)
{
	PyErr_SetString(PyExc_RuntimeError, "unimplemented");
	return NULL;
}

static PyObject *pyosd_command_set_set_member_attributes(PyObject *self,
						     PyObject *args)
{
	PyErr_SetString(PyExc_RuntimeError, "unimplemented");
	return NULL;
}

static PyObject *pyosd_command_set_write(PyObject *self,
						     PyObject *args)
{
	PyErr_SetString(PyExc_RuntimeError, "unimplemented");
	return NULL;
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
 * OSDAttr type.
 */
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

/*
 * OSDCommand instance methods, members, and type.
 */
struct PyMethodDef pyosd_command_methods[] = {
	{ "show_sense", pyosd_command_show_sense, METH_VARARGS,
		"Generate a string of returned sense data." },
	{ "set_inquiry", pyosd_command_set_inquiry, METH_VARARGS,
		"Build the INQURY command." },
	{ "set_append", pyosd_command_set_append, METH_VARARGS,
		"Build the APPEND command." },
	{ "set_create", pyosd_command_set_create, METH_VARARGS,
		"Build the CREATE command." },
	{ "set_create_and_write", pyosd_command_set_create_and_write,
		METH_VARARGS, "Build the CREATE_AND_WRITE command." },
	{ "set_create_collection", pyosd_command_set_create_collection,
		METH_VARARGS, "Build the CREATE_COLLECTION command." },
	{ "set_create_partition", pyosd_command_set_create_partition,
		METH_VARARGS, "Build the CREATE_PARTITION command." },
	{ "set_flush", pyosd_command_set_flush, METH_VARARGS,
		"Build the FLUSH command." },
	{ "set_flush_collection", pyosd_command_set_flush_collection,
		METH_VARARGS, "Build the FLUSH_COLLECTION command." },
	{ "set_flush_osd", pyosd_command_set_flush_osd, METH_VARARGS,
		"Build the FLUSH_OSD command." },
	{ "set_flush_partition", pyosd_command_set_flush_partition,
		METH_VARARGS, "Build the FLUSH_PARTITION command." },
	{ "set_format_osd", pyosd_command_set_format_osd, METH_VARARGS,
		"Build the FORMAT_OSD command." },
	{ "set_get_attributes", pyosd_command_set_get_attributes, METH_VARARGS,
		"Build the GET_ATTRIBUTES command." },
	{ "set_get_member_attributes", pyosd_command_set_get_member_attributes,
		METH_VARARGS, "Build the GET_MEMBER_ATTRIBUTES command." },
	{ "set_list", pyosd_command_set_list, METH_VARARGS,
		"Build the LIST command." },
	{ "set_list_collection", pyosd_command_set_list_collection,
		METH_VARARGS, "Build the LIST_COLLECTION command." },
	{ "set_perform_scsi_command", pyosd_command_set_perform_scsi_command,
		METH_VARARGS, "Build the PERFORM_SCSI_COMMAND command." },
	{ "set_perform_task_mgmt_func",
		pyosd_command_set_perform_task_mgmt_func, METH_VARARGS,
		"Build the PERFORM_SCSI_TASK_MGMT_FUNC command." },
	{ "set_query", pyosd_command_set_query, METH_VARARGS,
		"Build the QUERY command." },
	{ "set_read", pyosd_command_set_read, METH_VARARGS,
		"Build the READ command." },
	{ "set_remove", pyosd_command_set_remove, METH_VARARGS,
		"Build the REMOVE command." },
	{ "set_remove_collection", pyosd_command_set_remove_collection,
		METH_VARARGS, "Build the REMOVE_COLLECTION command." },
	{ "set_remove_member_objects", pyosd_command_set_remove_member_objects,
		METH_VARARGS, "Build the REMOVE_MEMBER_OBJECTS command." },
	{ "set_remove_partition", pyosd_command_set_remove_partition,
		METH_VARARGS, "Build the REMOVE_PARTITION command." },
	{ "set_set_attributes", pyosd_command_set_set_attributes, METH_VARARGS,
		"Build the SET_ATTRIBUTES command." },
	{ "set_set_key", pyosd_command_set_set_key, METH_VARARGS,
		"Build the SET_KEY command." },
	{ "set_set_master_key", pyosd_command_set_set_master_key, METH_VARARGS,
		"Build the SET_MASTER_KEY command." },
	{ "set_set_member_attributes", pyosd_command_set_set_member_attributes,
		METH_VARARGS, "Build the SET_MEMBER_ATTRIBUTES command." },
	{ "set_write", pyosd_command_set_write, METH_VARARGS,
		"Build the WRITE command." },
	{ "attr_build", pyosd_command_attr_build, METH_VARARGS,
		"Modify a command to get a particular attribute." },
	{ "attr_resolve", pyosd_command_attr_resolve, METH_VARARGS,
		"After command execution, process the retrieved attributes." },
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


/* picked up by the python module loader; this avoids a compilation warning */
PyMODINIT_FUNC initpyosd(void);

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

