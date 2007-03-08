/*
 * Drivelist interface.
 */
#include <Python.h>
#include <structmember.h>  /* T_OBJECT */
#include "osd_initiator/command.h"
#include "osd_initiator/drivelist.h"
#include "pyosd.h"

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

