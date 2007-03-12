/*
 * Constants from #defines and enums.  Try to keep this list up to date.
 */
#include <Python.h>
#include "osd_initiator/command.h"
#include "util/osd-defs.h"
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
	add(COLLECTIONS_PG);

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
	return ret;
}

