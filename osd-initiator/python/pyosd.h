/*
 * Declarations of objects found in all the source files.
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
/*
 * OSDAttr type.  Wrapper around one attribute_list entry.
 */
struct pyosd_attr {
	PyObject_HEAD;
	struct attribute_list attr;
};

/*
 * OSDCommand type.
 */
struct pyosd_command {
	PyObject_HEAD;
	struct osd_command command;
	int set;
	int complete;
};

extern PyTypeObject pyosd_command_type;
extern PyTypeObject pyosd_attr_type;
extern PyTypeObject pyosd_device_type;
extern PyTypeObject pyosd_drive_type;
extern PyTypeObject pyosd_drivelist_type;

/* exporting this to python */
PyMODINIT_FUNC initpyosd(void);

/* helper function in const.c */
int add_consts(PyObject *d);

