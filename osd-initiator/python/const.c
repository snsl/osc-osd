/*
 * Constants from #defines and enums.  Try to keep this list up to date.
 *
 * Copyright (C) 2007-8 OSD Team <pvfs-osd@osc.edu>
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
#include "osd-util/osd-defs.h"
#include "osd-util/osd-sense.h"
#include "pyosd.h"

/*
 * Insert a strings into a dictionary.
 */
static int dict_insert(PyObject *d, const char *name, unsigned long val)
{
        PyObject *v;
	
	v = PyLong_FromUnsignedLong(val);
	if (!v)
		return 1;

        if (PyDict_SetItemString(d, name, v) < 0)
                return 1;

        Py_DECREF(v);
        return 0;
}

#define add(s) \
	ret += dict_insert(d, #s, s)

/*
 * The object here is the main module dictionary.
 */
int add_consts(PyObject *d)
{
	int ret = 0;

	/* struct osd_command type */
	add(ATTR_GET);
	add(ATTR_GET_PAGE);
	add(ATTR_GET_MULTI);
	add(ATTR_SET);

	/* object constants */
	add(ROOT_PID);
	add(ROOT_OID);
	add(PARTITION_PID_LB);
	add(PARTITION_OID);
	add(USEROBJECT_PID_LB);
	add(COLLECTION_PID_LB);
	add(USEROBJECT_OID_LB);
	add(COLLECTION_OID_LB);

	/* attribute page ranges */
	add(USEROBJECT_PG);
	add(PARTITION_PG);
	add(COLLECTION_PG);
	add(ROOT_PG);
	add(RESERVED_PG);
	add(ANY_PG);
	add(CUR_CMD_ATTR_PG);
	add(GETALLATTR_PG);

	/* attribute page sets further subdivide page ranges */
	add(LUN_PG_LB);

	/* attribute pages defined by osd spec */
	add(USER_DIR_PG);
	add(USER_INFO_PG);
	add(USER_QUOTA_PG);
	add(USER_TMSTMP_PG);
	add(USER_COLL_PG);

	/* contents of current command attribute page */
	add(CCAP_RICV);
	add(CCAP_OBJT);
	add(CCAP_PID);
	add(CCAP_OID);
	add(CCAP_APPADDR);
	add(CCAP_RICV_LEN);
	add(CCAP_OBJT_LEN);
	add(CCAP_PID_LEN);
	add(CCAP_OID_LEN);
	add(CCAP_APPADDR_LEN);
	add(CCAP_RICV_OFF);
	add(CCAP_OBJT_OFF);
	add(CCAP_PID_OFF);
	add(CCAP_OID_OFF);
	add(CCAP_APPADDR_OFF);
	add(CCAP_TOTAL_LEN);

	/* userobject timestamp attribute page osd2r01 sec 7.1.2.18 */
	add(UTSAP_CTIME);
	add(UTSAP_ATTR_ATIME);
	add(UTSAP_ATTR_MTIME);
	add(UTSAP_DATA_ATIME);
	add(UTSAP_DATA_MTIME);
	add(UTSAP_CTIME_LEN);
	add(UTSAP_ATTR_ATIME_LEN);
	add(UTSAP_ATTR_MTIME_LEN);
	add(UTSAP_DATA_ATIME_LEN);
	add(UTSAP_DATA_MTIME_LEN);
	add(UTSAP_CTIME_OFF);
	add(UTSAP_ATTR_ATIME_OFF);
	add(UTSAP_ATTR_MTIME_OFF);
	add(UTSAP_DATA_ATIME_OFF);
	add(UTSAP_DATA_MTIME_OFF);
	add(UTSAP_TOTAL_LEN);

	/* userobject information attribute page osd2r01 sec 7.1.2.11 */
	add(UIAP_PID);
	add(UIAP_OID);
	add(UIAP_USERNAME);
	add(UIAP_USED_CAPACITY);
	add(UIAP_LOGICAL_LEN);
	add(UIAP_PID_LEN);
	add(UIAP_OID_LEN);
	add(UIAP_USED_CAPACITY_LEN);
	add(UIAP_LOGICAL_LEN_LEN);

	/* selected sense keys and codes */
	add(OSD_SSK_NO_SENSE);
	add(OSD_SSK_RECOVERED_ERROR);
	add(OSD_SSK_NOT_READY);
	add(OSD_SSK_MEDIUM_ERROR);
	add(OSD_SSK_HARDWARE_ERROR);
	add(OSD_SSK_ILLEGAL_REQUEST);
	add(OSD_SSK_UNIT_ATTENTION);
	add(OSD_SSK_DATA_PROTECTION);
	add(OSD_SSK_BLANK_CHECK);
	add(OSD_SSK_VENDOR_SPECIFIC);
	add(OSD_SSK_COPY_ABORTED);
	add(OSD_SSK_ABORTED_COMMAND);
	add(OSD_SSK_OBSOLETE_SENSE_KEY);
	add(OSD_SSK_VOLUME_OVERFLOW);
	add(OSD_SSK_MISCOMPARE);
	add(OSD_SSK_RESERVED_SENSE_KEY);

	add(OSD_ASC_INVALID_COMMAND_OPCODE);
	add(OSD_ASC_INVALID_DATA_OUT_BUF_INTEGRITY_CHK_VAL);
	add(OSD_ASC_INVALID_FIELD_IN_CDB);
	add(OSD_ASC_INVALID_FIELD_IN_PARAM_LIST);
	add(OSD_ASC_LOGICAL_UNIT_NOT_RDY_FRMT_IN_PRGRS);
	add(OSD_ASC_NONCE_NOT_UNIQUE);
	add(OSD_ASC_NONCE_TIMESTAMP_OUT_OF_RANGE);
	add(OSD_ASC_POWER_ON_OCCURRED);
	add(OSD_ASC_PARAMETER_LIST_LENGTH_ERROR);
	add(OSD_ASC_PART_OR_COLL_CONTAINS_USER_OBJECTS);
	add(OSD_ASC_READ_PAST_END_OF_USER_OBJECT);
	add(OSD_ASC_RESERVATIONS_RELEASED);
	add(OSD_ASC_QUOTA_ERROR);
	add(OSD_ASC_SECURITY_AUDIT_VALUE_FROZEN);
	add(OSD_ASC_SECURITY_WORKING_KEY_FROZEN);
	add(OSD_ASC_SYSTEM_RESOURCE_FAILURE);

	return ret;
}

