/*
 * Commands.
 */
#include <Python.h>
#include <structmember.h>  /* T_UBYTE */
#include "osd-initiator/command.h"
#include "osd-initiator/sense.h"
#include "osd-util/osd-util.h"
#include "pyosd.h"

/*
 * Clear everything.  Command will be cleared by a set function.
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

	if (command->attr) {
		osd_command_attr_resolve(command);
		osd_command_attr_free(command);
	}
	PyMem_Free(command->indata);
	PyMem_Free((void *)(uintptr_t) command->outdata);
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
	Py_RETURN_NONE;
}

static PyObject *pyosd_command_get_sense(PyObject *self, void *closure)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	const char *which = closure;
	int key, code;

	if (!py_command->complete) {
		PyErr_SetString(PyExc_RuntimeError, "command not complete");
		return NULL;
	}
	osd_sense_extract(command->sense, command->sense_len, &key, &code);
	if (strcmp(which, "key") == 0)
		return PyInt_FromLong(key);
	if (strcmp(which, "code") == 0)
		return PyInt_FromLong(code);
	Py_RETURN_NONE;
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
	s = osd_sense_as_string(command->sense, command->sense_len);
	o = Py_BuildValue("s", s);
	free(s);
	return o;
}

/*
 * Called once to linearize the list of attributes (or single).
 */
static struct attribute_list *attr_flatten(PyObject *o, int *numattr)
{
	struct pyosd_attr *py_attr;
	struct attribute_list *attr = NULL;
	*numattr = 0;

	if (PyObject_TypeCheck(o, &pyosd_attr_type)) {
		py_attr = (struct pyosd_attr *) o;
		attr = malloc(sizeof(*attr));
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

		attr = malloc(num * sizeof(*attr));
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
 * Attributes.  Valid argument to this function is OSDAttr or list of OSDAttr.
 */
static PyObject *pyosd_command_attr_build(PyObject *self, PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	struct attribute_list *attr;
	int numattr;
	PyObject *o;
	int ret;

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
	if (command->attr) {
		PyErr_SetString(PyExc_RuntimeError, "attributes already built");
		return NULL;
	}

	/*
	 * Pull attrs into a single flat allocated list and keep it around
	 * for resolve to write back into.
	 */
	attr = attr_flatten(o, &numattr);
	if (!attr)
		return NULL;

	/*
	 * Copies values out of attr but does not need to hold onto it.
	 */
	ret = osd_command_attr_build(command, attr, numattr);
	if (ret) {
		PyErr_SetString(PyExc_RuntimeError, "attr_build failed");
		return NULL;
	}
	free(attr);

	Py_IncRef(self);
	return self;
}

static PyObject *attr_build_manual(struct attribute_list *cmdattr)
{
	PyObject *o;
	struct pyosd_attr *py_attr;
	struct attribute_list *attr;

	o = pyosd_attr_type.tp_new(&pyosd_attr_type, NULL, NULL);
	if (!o)
		return PyErr_NoMemory();

	py_attr = (struct pyosd_attr *) o;
	attr = &py_attr->attr;
	memcpy(attr, cmdattr, sizeof(*attr));

	/* 0xffff means not found */
	attr->val = NULL;
	if (attr->outlen > 0 && attr->outlen < 0xffff) {
		attr->val = PyMem_Malloc(attr->outlen);
		if (!o)
			return PyErr_NoMemory();
		memcpy(attr->val, cmdattr->val, attr->outlen);
	}
	return o;
}

/*
 * Must be called after the command runs, and only if attr_build was
 * used.  Could hide this in the run, but want to keep the python and
 * C APIs similar.
 *
 * The attr list is kept with the command, but we force user to pass
 * it in again to keep the API similar to C.
 */
static PyObject *pyosd_command_attr_resolve(PyObject *self, PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	PyObject *o;
	int ret;

	if (!PyArg_ParseTuple(args, ":attr_resolve"))
		return NULL;

	if (!py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command not set");
		return NULL;
	}
	if (!py_command->complete) {
		PyErr_SetString(PyExc_RuntimeError, "command not complete");
		return NULL;
	}

	/* not an error */
	if (command->attr == NULL)
		Py_RETURN_NONE;

	ret = osd_command_attr_resolve(command);
	if (ret) {
		PyErr_SetString(PyExc_RuntimeError, "attr_resolve failed");
		return NULL;
	}

	/*
	 * Build new OSDAttr with output values.
	 */
	if (command->numattr == 0)
		o = Py_BuildValue("");
	else if (command->numattr == 1)
		o = attr_build_manual(command->attr);
	else {
		int i;
		o = PyList_New(command->numattr);
		for (i=0; i<command->numattr; i++) {
			PyObject *oi = attr_build_manual(&command->attr[i]);
			PyList_SetItem(o, i, oi);
		}
	}
	return o;
}

/*
 * One of these for each of the major set functions.
 */
static PyObject *pyosd_command_set_inquiry(PyObject *self, PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	unsigned int page_code = 0;

	if (!PyArg_ParseTuple(args, "|I:inquiry", &page_code))
		return NULL;
	if (py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command already set");
		return NULL;
	}

	py_command->set = 1;
	osd_command_set_inquiry(command, page_code, 80);
	command->indata = PyMem_Malloc(80);
	if (command->indata == NULL)
		return PyErr_NoMemory();
	command->inlen_alloc = 80;
	Py_IncRef(self);
	return self;
}

static PyObject *pyosd_command_set_append(PyObject *self, PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	uint8_t *buf;
	uint64_t pid, oid, len;

	if (!PyArg_ParseTuple(args, "KKs#:set_append", &pid, &oid, &buf, &len))
		return NULL;
	if (py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command already set");
		return NULL;
	}

	py_command->set = 1;
	osd_command_set_append(command, pid, oid, len);
	command->outdata = PyMem_Malloc(len);
	if (!command->outdata)
		return PyErr_NoMemory();
	memcpy((void *)(uintptr_t) command->outdata, buf, len);
	command->outlen = len;
	Py_IncRef(self);
	return self;
}

static PyObject *pyosd_command_set_create(PyObject *self, PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	uint64_t pid = 0, oid = 0, numobj = 1;

	if (!PyArg_ParseTuple(args, "K|KK:set_create", &pid, &oid, &numobj))
		return NULL;
	if (py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command already set");
		return NULL;
	}

	py_command->set = 1;
	osd_command_set_create(command, pid, oid, numobj);
	Py_IncRef(self);
	return self;
}

static PyObject *pyosd_command_set_create_and_write(PyObject *self,
						    PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	uint8_t *buf;
	uint64_t pid, oid, len, offset = 0;

	if (!PyArg_ParseTuple(args, "KKs#|K:set_create_and_write", &pid, &oid,
			      &buf, &len, &offset))
		return NULL;
	if (py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command already set");
		return NULL;
	}

	py_command->set = 1;
	osd_command_set_create_and_write(command, pid, oid, len, offset);
	command->outdata = PyMem_Malloc(len);
	if (!command->outdata)
		return PyErr_NoMemory();
	/* XXX: take a ref on buf rather than copying it */
	memcpy((void *)(uintptr_t) command->outdata, buf, len);
	command->outlen = len;
	Py_IncRef(self);
	return self;
}

static PyObject *pyosd_command_set_create_collection(PyObject *self,
						     PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	uint64_t pid = 0, cid = 0;

	if (!PyArg_ParseTuple(args, "K|K:set_create_collection", &pid, &cid))
		return NULL;
	if (py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command already set");
		return NULL;
	}

	py_command->set = 1;
	osd_command_set_create_collection(command, pid, cid);
	Py_IncRef(self);
	return self;
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
	Py_IncRef(self);
	return self;
}

static PyObject *pyosd_command_set_flush(PyObject *self, PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	uint64_t pid, oid;
	int scope;

	if (!PyArg_ParseTuple(args, "KKi:set_flush", &pid, &oid, &scope))
		return NULL;
	if (py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command already set");
		return NULL;
	}

	py_command->set = 1;
	osd_command_set_flush(command, pid, oid, scope);
	Py_IncRef(self);
	return self;
}

static PyObject *pyosd_command_set_flush_collection(PyObject *self,
						    PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	uint64_t pid, cid;
	int scope;

	if (!PyArg_ParseTuple(args, "KKi:set_flush_collection", &pid, &cid,
			      &scope))
		return NULL;
	if (py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command already set");
		return NULL;
	}

	py_command->set = 1;
	osd_command_set_flush_collection(command, pid, cid, scope);
	Py_IncRef(self);
	return self;
}

static PyObject *pyosd_command_set_flush_osd(PyObject *self, PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	int scope;

	if (!PyArg_ParseTuple(args, "i:set_flush_osd", &scope))
		return NULL;
	if (py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command already set");
		return NULL;
	}

	py_command->set = 1;
	osd_command_set_flush_osd(command, scope);
	Py_IncRef(self);
	return self;
}

static PyObject *pyosd_command_set_flush_partition(PyObject *self,
						   PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	uint64_t pid;
	int scope;

	if (!PyArg_ParseTuple(args, "KKi:set_flush_partition", &pid, &scope))
		return NULL;
	if (py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command already set");
		return NULL;
	}

	py_command->set = 1;
	osd_command_set_flush_partition(command, pid, scope);
	Py_IncRef(self);
	return self;
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
	Py_IncRef(self);
	return self;
}

static PyObject *pyosd_command_set_get_attributes(PyObject *self,
						  PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	uint64_t pid, oid;

	if (!PyArg_ParseTuple(args, "KK:set_get_attributes", &pid, &oid))
		return NULL;
	if (py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command already set");
		return NULL;
	}

	py_command->set = 1;
	osd_command_set_get_attributes(command, pid, oid);
	Py_IncRef(self);
	return self;
}

static PyObject *pyosd_command_set_get_member_attributes(PyObject *self,
							 PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	uint64_t pid, cid;

	if (!PyArg_ParseTuple(args, "KK:set_get_member_attributes", &pid, &cid))
		return NULL;
	if (py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command already set");
		return NULL;
	}

	py_command->set = 1;
	osd_command_set_get_member_attributes(command, pid, cid);
	Py_IncRef(self);
	return self;
}

/*
 * XXX: This is likely the wrong interface for python.
 */
static PyObject *pyosd_command_set_list(PyObject *self, PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	uint64_t pid, alloc_len, initial_oid;
	uint32_t list_id;
	int list_attr;

	if (!PyArg_ParseTuple(args, "KIKKi:set_list", &pid, &list_id,
			      &alloc_len, &initial_oid, &list_attr))
		return NULL;
	if (py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command already set");
		return NULL;
	}

	py_command->set = 1;
	osd_command_set_list(command, pid, list_id, alloc_len, initial_oid,
			     list_attr);
	Py_IncRef(self);
	return self;
}

static PyObject *pyosd_command_set_list_collection(PyObject *self,
						   PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	uint64_t pid, cid, alloc_len, initial_oid;
	uint32_t list_id;

	if (!PyArg_ParseTuple(args, "KIKK:set_list_collection", &pid, &cid,
			      &list_id, &alloc_len, &initial_oid))
		return NULL;
	if (py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command already set");
		return NULL;
	}

	py_command->set = 1;
	osd_command_set_list_collection(command, pid, cid, list_id, alloc_len,
					initial_oid, 0);
	Py_IncRef(self);
	return self;
}

static PyObject *pyosd_command_set_perform_scsi_command(PyObject *self __unused,
							PyObject *args __unused)
{
	PyErr_SetString(PyExc_RuntimeError, "unimplemented");
	return NULL;
}

static PyObject *pyosd_command_set_perform_task_mgmt_func(PyObject *self __unused,
							  PyObject *args __unused)
{
	PyErr_SetString(PyExc_RuntimeError, "unimplemented");
	return NULL;
}

static PyObject *pyosd_command_set_query(PyObject *self, PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	uint64_t pid, cid, alloc_len;
	uint32_t query_len;

	if (!PyArg_ParseTuple(args, "KKIK:set_query", &pid, &cid, &query_len,
			      &alloc_len))
		return NULL;
	if (py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command already set");
		return NULL;
	}

	py_command->set = 1;
	osd_command_set_query(command, pid, cid, query_len, alloc_len);
	Py_IncRef(self);
	return self;
}

static PyObject *pyosd_command_set_read(PyObject *self, PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	uint64_t pid, oid, len, offset = 0;

	if (!PyArg_ParseTuple(args, "KKK|K:set_read", &pid, &oid, &len,
			      &offset))
		return NULL;
	if (py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command already set");
		return NULL;
	}

	py_command->set = 1;
	osd_command_set_read(command, pid, oid, len, offset);
	command->indata = PyMem_Malloc(len);
	command->inlen_alloc = len;
	Py_IncRef(self);
	return self;
}

static PyObject *pyosd_command_set_remove(PyObject *self, PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	uint64_t pid, oid;

	if (!PyArg_ParseTuple(args, "KK:set_remove", &pid, &oid))
		return NULL;
	if (py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command already set");
		return NULL;
	}

	py_command->set = 1;
	osd_command_set_remove(command, pid, oid);
	Py_IncRef(self);
	return self;
}

static PyObject *pyosd_command_set_remove_collection(PyObject *self,
						     PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	uint64_t pid, cid;
	int force = 0;

	if (!PyArg_ParseTuple(args, "KK|i:set_remove_collection", &pid, &cid,
			      &force))
		return NULL;
	if (py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command already set");
		return NULL;
	}

	py_command->set = 1;
	osd_command_set_remove_collection(command, pid, cid, force);
	Py_IncRef(self);
	return self;
}

static PyObject *pyosd_command_set_remove_member_objects(PyObject *self,
							 PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	uint64_t pid, cid;

	if (!PyArg_ParseTuple(args, "KK:set_remove_member_objects", &pid, &cid))
		return NULL;
	if (py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command already set");
		return NULL;
	}

	py_command->set = 1;
	osd_command_set_remove_member_objects(command, pid, cid);
	Py_IncRef(self);
	return self;
}

static PyObject *pyosd_command_set_remove_partition(PyObject *self,
						    PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	uint64_t pid;

	if (!PyArg_ParseTuple(args, "K:set_remove_partition", &pid))
		return NULL;
	if (py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command already set");
		return NULL;
	}

	py_command->set = 1;
	osd_command_set_remove_partition(command, pid);
	Py_IncRef(self);
	return self;
}

static PyObject *pyosd_command_set_set_attributes(PyObject *self,
						  PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	uint64_t pid, oid;

	if (!PyArg_ParseTuple(args, "KK:set_set_attributes", &pid, &oid))
		return NULL;
	if (py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command already set");
		return NULL;
	}

	py_command->set = 1;
	osd_command_set_set_attributes(command, pid, oid);
	Py_IncRef(self);
	return self;
}

static PyObject *pyosd_command_set_set_key(PyObject *self,
					   PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	int key_to_set;
	uint64_t pid, key;
	const uint8_t *seed;
	int len;

	if (!PyArg_ParseTuple(args, "iKKs#:set_set_key", &key_to_set, &pid,
			      &key, &seed, &len))
		return NULL;
	if (py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command already set");
		return NULL;
	}

	py_command->set = 1;
	if (len != 20) {
		PyErr_SetString(PyExc_RuntimeError, "key len should be 20");
		return NULL;
	}
	osd_command_set_set_key(command, key_to_set, pid, key, seed);
	Py_IncRef(self);
	return self;
}

static PyObject *pyosd_command_set_set_master_key(PyObject *self,
						  PyObject *args)
{
	return NULL;
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	int dh_step;
	uint64_t key;
	uint32_t param_len, alloc_len;

	if (!PyArg_ParseTuple(args, "iKII:set_set_master_key", &dh_step, &key,
			      &param_len, &alloc_len))
		return NULL;
	if (py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command already set");
		return NULL;
	}

	py_command->set = 1;
	osd_command_set_set_master_key(command, dh_step, key, param_len,
				       alloc_len);
	Py_IncRef(self);
	return self;
}

static PyObject *pyosd_command_set_set_member_attributes(PyObject *self,
							 PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	uint64_t pid, cid;

	if (!PyArg_ParseTuple(args, "KK:set_set_member_attributes", &pid, &cid))
		return NULL;
	if (py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command already set");
		return NULL;
	}

	py_command->set = 1;
	osd_command_set_set_member_attributes(command, pid, cid);
	Py_IncRef(self);
	return self;
}

static PyObject *pyosd_command_set_write(PyObject *self, PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	const char *buf;
	int len;
	uint64_t pid, oid, offset = 0;

	if (!PyArg_ParseTuple(args, "KKs#|K:set_write", &pid, &oid, &buf, &len,
			      &offset))
		return NULL;
	if (py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command already set");
		return NULL;
	}

	py_command->set = 1;
	osd_command_set_write(command, pid, oid, len, offset);
	command->outdata = PyMem_Malloc(len);
	if (!command->outdata)
		return PyErr_NoMemory();
	memcpy((void *)(uintptr_t) command->outdata, buf, len);
	command->outlen = len;
	Py_IncRef(self);
	return self;
}

/*
 * Input:  two values, compare and swap; output: original value.
 */
static PyObject *pyosd_command_set_cas(PyObject *self, PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	uint64_t pid, oid, len = 8, offset = 0;
	const char *inbuf, *outbuf;
	int inlen, outlen;

	if (!PyArg_ParseTuple(args, "KKs#s#|KK:set_cas", &pid, &oid,
			      inbuf, inlen, outbuf, outlen, &len, &offset))
		return NULL;
	if (py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command already set");
		return NULL;
	}

	/*
	 * Various asserts.  inlen = 2 * len; outlen = len;
	 */
	py_command->set = 1;
	osd_command_set_cas(command, pid, oid, len, offset);
	command->indata = PyMem_Malloc(len);
	if (!command->indata)
		return PyErr_NoMemory();
	memcpy((void *)(uintptr_t) command->outdata, outbuf, outlen);
	command->outlen = outlen;
	Py_IncRef(self);
	return self;
}

/*
 * Input: one value, add; output: original value.
 */
static PyObject *pyosd_command_set_fa(PyObject *self, PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	uint64_t pid, oid, len = 8, offset = 0;
	const char *inbuf, *outbuf;
	int inlen, outlen;

	if (!PyArg_ParseTuple(args, "KKs#s#|KK:set_fa", &pid, &oid,
			      inbuf, inlen, outbuf, outlen, &len, &offset))
		return NULL;
	if (py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command already set");
		return NULL;
	}

	/*
	 * Various asserts.  inlen = outlen = len;
	 */
	py_command->set = 1;
	osd_command_set_cas(command, pid, oid, len, offset);
	command->indata = PyMem_Malloc(len);
	if (!command->indata)
		return PyErr_NoMemory();
	memcpy((void *)(uintptr_t) command->outdata, outbuf, outlen);
	command->outlen = outlen;
	Py_IncRef(self);
	return self;
}
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
	/*
	 * Local extensions.
	 */
	{ "set_cas", pyosd_command_set_cas, METH_VARARGS,
		"Build the CAS (compare and swap) command." },
	{ "set_fa", pyosd_command_set_fa, METH_VARARGS,
		"Build the FA (fetch and add) command." },

	/*
	 * Attribute manipulation.
	 */
	{ "attr_build", pyosd_command_attr_build, METH_VARARGS,
		"Modify a command to get or set attributes." },
	{ "attr_resolve", pyosd_command_attr_resolve, METH_VARARGS,
		"After command execution, process the retrieved attributes.\n"
		"You must call this even if you are only using ATTR_SET." },
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
	{ "sense_key", pyosd_command_get_sense, NULL, "sense key", "key" },
	{ "sense_code", pyosd_command_get_sense, NULL, "sense code", "code" },
};

PyTypeObject pyosd_command_type = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = "OSDCommand",
	.tp_basicsize = sizeof(struct pyosd_command),
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_doc = "Python encapsulation of struct osd_command.",
	.tp_init = pyosd_command_init,
	.tp_new = PyType_GenericNew,
	.tp_dealloc = pyosd_command_dealloc,
	.tp_methods = pyosd_command_methods,
	.tp_members = pyosd_command_members,
	.tp_getset = pyosd_command_getset,
};

