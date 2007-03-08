/*
 * Commands.
 */
#include <Python.h>
#include <structmember.h>  /* T_UBYTE */
#include "osd_initiator/command.h"
#include "osd_initiator/sense.h"
#include "util/util.h"
#include "pyosd.h"

/*
 * Clear everything.
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
	osd_command_set_create(command, pid, oid, 1);
	return Py_BuildValue("");
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
	return Py_BuildValue("");
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
	osd_command_set_create_partition(command, pid);
	return Py_BuildValue("");
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
	return Py_BuildValue("");
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
	return Py_BuildValue("");
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
	return Py_BuildValue("");
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
	osd_command_set_format_osd(command, capacity);
	return Py_BuildValue("");
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
	return Py_BuildValue("");
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
	return Py_BuildValue("");
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

	if (!PyArg_ParseTuple(args, "KIKK:set_list", &pid, &list_id, &alloc_len,
			      &initial_oid))
		return NULL;
	if (py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command already set");
		return NULL;
	}

	py_command->set = 1;
	osd_command_set_list(command, pid, list_id, alloc_len, initial_oid);
	return Py_BuildValue("");
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
					initial_oid);
	return Py_BuildValue("");
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
	return Py_BuildValue("");
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
	return Py_BuildValue("");
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
	return Py_BuildValue("");
}

static PyObject *pyosd_command_set_remove_collection(PyObject *self,
						     PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	uint64_t pid, cid;

	if (!PyArg_ParseTuple(args, "KK:set_remove_collection", &pid, &cid))
		return NULL;
	if (py_command->set) {
		PyErr_SetString(PyExc_RuntimeError, "command already set");
		return NULL;
	}

	py_command->set = 1;
	osd_command_set_remove_collection(command, pid, cid);
	return Py_BuildValue("");
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
	return Py_BuildValue("");
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
	return Py_BuildValue("");
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
	return Py_BuildValue("");
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
	return Py_BuildValue("");
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
	return Py_BuildValue("");
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
	return Py_BuildValue("");
}

static PyObject *pyosd_command_set_write(PyObject *self, PyObject *args)
{
	struct pyosd_command *py_command = (struct pyosd_command *) self;
	struct osd_command *command = &py_command->command;
	uint8_t *buf;
	uint64_t pid, oid, len, offset = 0;

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
	return Py_BuildValue("");
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

