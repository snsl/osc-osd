#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <assert.h>

#include "osd.h"
#include "osd-defs.h"
#include "db.h"
#include "attr.h"
#include "util/util.h"
#include "obj.h"
#include "osd-sense.h"

static const char *dbname = "osd.db";
static const char *dfiles = "dfiles";
static const char *stranded = "stranded";

extern const char osd_schema[];

static struct init_attr root_info[] = {
	{ ROOT_PG + 0, 0, "INCITS  T10 Root Directory" },
	{ ROOT_PG + 0, ROOT_PG + 1, "INCITS  T10 Root Information" },
	{ ROOT_PG + 1, 0, "INCITS  T10 Root Information" },
	{ ROOT_PG + 1, 3, "\xf1\x81\x00\x0eOSC     OSDEMU" },
	{ ROOT_PG + 1, 4, "OSC" },
	{ ROOT_PG + 1, 5, "OSDEMU" },
	{ ROOT_PG + 1, 6, "9001" },
	{ ROOT_PG + 1, 7, "0" },
	{ ROOT_PG + 1, 8, "1" },
	{ ROOT_PG + 1, 9, "hostname" },
	{ ROOT_PG + 0, ROOT_PG + 2, "INCITS  T10 Root Quotas" },
	{ ROOT_PG + 2, 0, "INCITS  T10 Root Quotas" },
	{ ROOT_PG + 0, ROOT_PG + 3, "INCITS  T10 Root Timestamps" },
	{ ROOT_PG + 3, 0, "INCITS  T10 Root Timestamps" },
	{ ROOT_PG + 0, ROOT_PG + 5, "INCITS  T10 Root Policy/Security" },
	{ ROOT_PG + 5, 0, "INCITS  T10 Root Policy/Security" },
};

static struct init_attr partition_info[] = {
	{ PARTITION_PG + 0, 0, "INCITS  T10 Partition Directory" },
	{ PARTITION_PG + 0, PARTITION_PG + 1, 
		"INCITS  T10 Partition Information" },
	{ PARTITION_PG + 1, 0, "INCITS  T10 Partition Information" },
};

static int create_dir(const char *dirname)
{
	int ret = 0;
	struct stat sb;

	ret = stat(dirname, &sb);
	if (ret == 0) {
		if (!S_ISDIR(sb.st_mode)) {
			error("%s: dirname %s not a directory", __func__, 
			      dirname);
			return -ENOTDIR;
		}
	} else {
		if (errno != ENOENT) {
			error("%s: stat dirname %s", __func__, dirname);
			return -ENOTDIR;
		}

		/* create dir*/
		ret = mkdir(dirname, 0777);
		if (ret < 0) {
			error("%s: create dirname %s", __func__, dirname);
			return ret;
		}
	}

	return 0;
}

/* only empties 1-level deep directories */ 
static int empty_dir(const char *dirname)
{
	int ret = 0;
	char path[MAXNAMELEN];
	DIR *dir = NULL;
	struct dirent *ent = NULL;
	
	if ((dir = opendir(dirname)) == NULL)
		return -1;

	while ((ent = readdir(dir)) != NULL) {
		if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
			continue;
		/* NOTE: no need to memset path with zeros */
		sprintf(path, "%s/%s", dirname, ent->d_name);
		ret = unlink(path);
		if (ret != 0)
			return -1;
	}
	ret = rmdir(dirname);
	if (ret != 0)
		return -1;

	return 0;
}

static inline void get_dfile_name(char *path, const char *root,
				  uint64_t pid, uint64_t oid)
{
	sprintf(path, "%s/%s/%llu.%llu", root, dfiles, llu(pid), llu(oid));
}

static inline void fill_ccap(struct cur_cmd_attr_pg *ccap, uint16_t srvc_act,
			     uint8_t *ricv, uint8_t obj_type, uint64_t pid,
			     uint64_t oid, uint64_t append_off) 
{
	memset(ccap, 0, sizeof(*ccap));
	ccap->cdb_srvc_act = srvc_act;
	if (ricv)
		memcpy(ccap->ricv, ricv, sizeof(ccap->ricv));
	ccap->obj_type = obj_type;
	ccap->pid = pid;
	ccap->oid = oid;
	ccap->append_off = append_off;
}

static inline void fill_attr(void *buf, uint32_t page, uint32_t number, 
			     uint16_t len, void *val)
{
	list_entry_t *le = (list_entry_t *)buf;
	set_htonl_le((uint8_t *)&(le->page),  page);
	set_htonl_le((uint8_t *)&(le->number), number);
	set_htons_le((uint8_t *)&(le->len), len);
	memcpy((uint8_t *)le + ATTR_VAL_OFFSET, val, len);
}

/*
 * Fill current command attributes page (osd2r00 Sec 7.1.2.24) in retrieved
 * attribute format described in osd2r00 Sec 7.1.3.3
 */
static int get_ccap(struct osd_device *osd, void *outbuf, uint16_t len)
{
	int ret = 0;
	size_t sz = 0;
	uint8_t *cp = outbuf;
	char page_id[CCAP_ID_LEN];

	/*
	 * If len < CCAP_LIST_LEN, i.e. the buffer size is not sufficient to
	 * fill in all attributes, then it is not an error. osd2r00 Sec
	 * 5.2.2.2
	 */
	if (osd == NULL || outbuf == NULL)
		return -EINVAL;

	sz = (CCAP_ID_LEN + ATTR_VAL_OFFSET);
	if (len < sz)
		goto out;
	sprintf(page_id, "INCITS  T10 Current Command");
	fill_attr(cp, CUR_CMD_ATTR_PG, 0x0, 40, page_id);
	cp += sz, len -= sz;
	
	sz = (sizeof(osd->ccap.ricv) + ATTR_VAL_OFFSET);
	if (len < sz)
		goto out;
	fill_attr(cp, CUR_CMD_ATTR_PG, 0x1, sizeof(osd->ccap.ricv), 
		  osd->ccap.ricv);
	cp += sz, len -= sz;

	sz = (sizeof(osd->ccap.obj_type) + ATTR_VAL_OFFSET);
	if (len < sz)
		goto out;
	fill_attr(cp, CUR_CMD_ATTR_PG, 0x2, sizeof(osd->ccap.obj_type), 
		  &osd->ccap.obj_type);
	cp += sz, len -= sz;

	sz = (sizeof(osd->ccap.pid) + ATTR_VAL_OFFSET);
	if (len < sz)
		goto out;
	fill_attr(cp, CUR_CMD_ATTR_PG, 0x3, sizeof(osd->ccap.pid), 
		  &osd->ccap.pid);
	cp += sz, len -= sz;

	sz = (sizeof(osd->ccap.oid) + ATTR_VAL_OFFSET);
	if (len < sz)
		goto out;
	fill_attr(cp, CUR_CMD_ATTR_PG, 0x4, sizeof(osd->ccap.oid), 
		  &osd->ccap.oid);
	cp += sz, len -= sz;

	sz = (sizeof(osd->ccap.append_off) + ATTR_VAL_OFFSET);
	if (len < sz)
		goto out;
	fill_attr(cp, CUR_CMD_ATTR_PG, 0x5, sizeof(osd->ccap.append_off), 
		  &osd->ccap.append_off);
	cp += sz, len -= sz;

	assert(cp - CCAP_LIST_LEN == (uint8_t *)outbuf);

out:
	return 0;
}

/*
 * Create root object and set attributes for root and partition zero.
 * = 0: success
 * !=0: failure
 */
static int osd_initialize_db(struct osd_device *osd)
{
	int i = 0;
	int ret = 0;
	uint64_t pid = 0;
	char *err = NULL;

	if (!osd)
		return -EINVAL;

	/* Build tables from schema file.  */
	ret = sqlite3_exec(osd->db, osd_schema, NULL, NULL, &err);
	if (ret != SQLITE_OK) {
		sqlite3_free(err);
		return -1;
	}

	ret = obj_insert(osd->db, ROOT_PID, ROOT_OID, ROOT);
	if (ret != SQLITE_OK)
		goto out;

	/* set root object attributes */
	for (i=0; i<ARRAY_SIZE(root_info); i++) {
		struct init_attr *ia = &root_info[i];
		ret = attr_set_attr(osd->db, ROOT_PID , ROOT_OID, ia->page, 
				    ia->number, ia->s, strlen(ia->s)+1);
		if (ret != SQLITE_OK)
			goto out;
	}

	/*
	 * partition zero (0,0) has the same object identifier as root object
	 * (0,0).  The attributes will not be overwritten since attr pages
	 * are different.
	 */
	for (i=0; i<ARRAY_SIZE(partition_info); i++) {
		struct init_attr *ia = &partition_info[i];
		ret = attr_set_attr(osd->db, ROOT_PID, ROOT_OID, ia->page,
				    ia->number, ia->s, strlen(ia->s)+1);
		if (ret)
			goto out;
	}

	/* assign pid as attr, osd2r00 Section 7.1.2.9 table 92  */
	ret = attr_set_attr(osd->db, ROOT_PID, ROOT_OID, PARTITION_PG + 1, 1, 
			    &pid, sizeof(pid));

out:
	return ret;
}

int osd_open(const char *root, struct osd_device *osd)
{
	int ret;
	char path[MAXNAMELEN];

	progname = "osd-target";  /* for debug messages from libosdutil */
	if (strlen(root) > MAXROOTLEN) {
		ret = -ENAMETOOLONG;
		goto out;
	}

	/* test if root exists and is a directory */
	ret = create_dir(root);
	if (ret != 0)
		goto out;

	/* create 'dfiles' sub-directory */
	memset(path, 0, MAXNAMELEN);
	sprintf(path, "%s/%s/", root, dfiles);
	ret = create_dir(path);
	if (ret != 0)
		goto out;

	/* create 'stranded-files' sub-directory */
	memset(path, 0, MAXNAMELEN);
	sprintf(path, "%s/%s/", root, stranded);
	ret = create_dir(path);
	if (ret != 0)
		goto out;

	/* auto-creates db if necessary, and sets osd->db */
	osd->root = strdup(root);
	sprintf(path, "%s/%s", root, dbname);
	ret = db_open(path, osd);
	if (ret == 1)
		ret = osd_initialize_db(osd);

out:
	return ret;
}

int osd_close(struct osd_device *osd)
{
	int ret;
	
	ret = db_close(osd);
	if (ret != 0)
		error("%s: db_close", __func__);
	free(osd->root);
	return ret;
}

/*
 * Externally callable error response generators.
 */
int osd_error_unimplemented(uint16_t action, uint8_t *sense)
{
	int ret;

	debug(__func__);
	ret = sense_basic_build(sense, OSD_SSK_ILLEGAL_REQUEST, 0, 0, 0, 0);
	return ret;
}

int osd_error_bad_cdb(uint8_t *sense)
{
	int ret;

	debug(__func__);
	/* should probably add what part of the cdb was bad */
	ret = sense_basic_build(sense, OSD_SSK_ILLEGAL_REQUEST, 0, 0, 0, 0);
	return ret;
}

/*
 * Commands
 */
int osd_append(struct osd_device *osd, uint64_t pid, uint64_t oid,
               uint64_t len, uint8_t *data, uint8_t *sense)
{
	int ret;

	debug(__func__);
	ret = sense_basic_build(sense, OSD_SSK_ILLEGAL_REQUEST, 0, 0, pid, oid);
	return ret;
}


static int osd_create_datafile(struct osd_device *osd, uint64_t pid, 
			       uint64_t oid)
{
	int ret = 0;
	char path[MAXNAMELEN];
	struct stat sb;

	get_dfile_name(path, osd->root, pid, oid);
	ret = stat(path, &sb);
	if (ret == 0 && S_ISREG(sb.st_mode)) {
		return -EEXIST;
	} else if (ret == -1 && errno == ENOENT) {
		ret = creat(path, 0666);
		if (ret <= 0)
			return ret;
		close(ret);
	} else {
		return ret;
	}

	return 0;
}

/*
 * XXX: get/set attributes to be handled in cdb.c
 */
int osd_create(struct osd_device *osd, uint64_t pid, uint64_t requested_oid,
	       uint16_t num, uint8_t *sense)
{
	int ret = 0;
	uint64_t i = 0;
	uint64_t oid = 0;

	debug("%s: pid %llu requested oid %llu num %hu", __func__, llu(pid),
	      llu(requested_oid), num);

	if (pid == 0 || pid < USEROBJECT_PID_LB) 	
		goto out_illegal_req;

	if (requested_oid != 0 && requested_oid < USEROBJECT_OID_LB) 	
		goto out_illegal_req;

	if (obj_ispresent(osd->db, pid, PARTITION_OID) == 0)
		goto out_illegal_req;

	if (num > 1 && requested_oid != 0) 
		goto out_illegal_req;

	if (requested_oid == 0) {
		ret = obj_get_nextoid(osd->db, pid, &oid);
		if (ret != 0) 
			goto out_hw_err;
		if (oid == 1)
			oid = USEROBJECT_OID_LB; /* first oid in partition */
	} else {
		ret = obj_ispresent(osd->db, pid, requested_oid);
		if (ret == 1) 
			goto out_illegal_req; /* requested_oid exists! */
		oid = requested_oid; /* requested_oid works! */
	}

	if (num == 0)
		num = 1; /* create atleast one object */

	for (i = oid; i < (oid + num); i++) {
		ret = obj_insert(osd->db, pid, i, USEROBJECT);
		if (ret != 0) {
			ret = sense_build_sdd(sense, OSD_SSK_HARDWARE_ERROR, 
					      OSD_ASC_INVALID_FIELD_IN_CDB, 
					      pid, i);
			return ret;
		}
		ret = osd_create_datafile(osd, pid, i);
		if (ret != 0) {
			/* TODO: delete previously created objects */
			obj_delete(osd->db, pid, i); 
			ret = sense_build_sdd(sense, OSD_SSK_HARDWARE_ERROR, 
					      OSD_ASC_INVALID_FIELD_IN_CDB, 
					      pid, i);
			return ret;
		}
	}

	return 0; /* success */
	
out_illegal_req:
	ret = sense_build_sdd(sense, OSD_SSK_ILLEGAL_REQUEST, 
			      OSD_ASC_INVALID_FIELD_IN_CDB, 
			      pid, requested_oid);
	return ret;

out_hw_err:
	ret = sense_build_sdd(sense, OSD_SSK_HARDWARE_ERROR, 
			      OSD_ASC_INVALID_FIELD_IN_CDB, 
			      pid, requested_oid);
	return ret;
}


int osd_create_and_write(struct osd_device *osd, uint64_t pid,
			 uint64_t requested_oid, uint64_t len, uint64_t offset,
			 uint8_t *data, uint8_t *sense)
{
	debug(__func__);
	return 0;
}


int osd_create_collection(struct osd_device *osd, uint64_t pid,
			  uint64_t requested_cid, uint8_t *sense)
{
	debug(__func__);
	return 0;
}

int osd_create_partition(struct osd_device *osd, uint64_t requested_pid,
                         uint8_t *sense)
{
	int ret = 0;
	uint64_t pid = 0;
	uint8_t ricv[20];

	debug("%s: pid %llu", __func__, llu(pid));

	if (requested_pid != 0 && requested_pid < PARTITION_PID_LB) 
		goto out_cdb_err;

	if (requested_pid == 0) {
		ret = obj_get_nextpid(osd->db, &pid);
		if (ret != 0)
			goto out_hw_err;
		if (pid == 1) 
			pid = PARTITION_PID_LB; /* firstever pid */
	} else {
		pid = requested_pid;
	}

	/* if pid already exists, obj_insert will fail */
	ret = obj_insert(osd->db, pid, PARTITION_OID, PARTITION);
	if (ret)
		goto out_cdb_err;

	fill_ccap(&(osd->ccap), OSD_CREATE_PARTITION, NULL, PARTITION, pid, 
		  PARTITION_OID, 0);
	return ret; /* success */

out_cdb_err:
	ret = sense_build_sdd(sense, OSD_SSK_ILLEGAL_REQUEST, 
			      OSD_ASC_INVALID_FIELD_IN_CDB, 
			      pid, requested_pid);
	return ret;

out_hw_err:
	ret = sense_build_sdd(sense, OSD_SSK_HARDWARE_ERROR, 
			      OSD_ASC_INVALID_FIELD_IN_CDB, 
			      pid, requested_pid);
	return ret;
}

int osd_flush(struct osd_device *osd, uint64_t pid, uint64_t oid,
	      int flush_scope, uint8_t *sense)
{
	debug(__func__);
	return 0;
}


int osd_flush_collection(struct osd_device *osd, uint64_t pid, uint64_t cid,
                         int flush_scope, uint8_t *sense)
{
	debug(__func__);
	return 0;
}


int osd_flush_osd(struct osd_device *osd, int flush_scope, uint8_t *sense)
{
	debug(__func__);
	return 0;
}


int osd_flush_partition(struct osd_device *osd, uint64_t pid, int flush_scope,
                        uint8_t *sense)
{
	debug(__func__);
	return 0;
}

/*
 * Destroy the db and start over again.
 */
int osd_format_osd(struct osd_device *osd, uint64_t capacity, uint8_t *sense)
{
	int ret;
	char *root;
	char path[MAXNAMELEN];
	struct stat sb;
	
	debug("%s: capacity %llu MB", __func__, llu(capacity >> 20));

	root = strdup(osd->root); 
	
	sprintf(path, "%s/%s", root, dbname);
	if(stat(path, &sb) != 0) {
		error_errno("%s: DB does not exist, creating it", __func__);
		goto create;
	}
	
	ret = db_close(osd); 
	if (ret) {
		error("%s: DB close failed, ret %d", __func__, ret);
		goto out_sense;
	}
	
	ret = unlink(path);
	if (ret) {
		error_errno("%s: unlink db %s", __func__, path);
		goto out_sense;
	}
	
	sprintf(path, "%s/%s", root, stranded);
	ret = empty_dir(path);
	if (ret) {
		error("%s: empty_dir %s failed", __func__, path);
		goto out_sense;
	}

	sprintf(path, "%s/%s", root, dfiles);
	ret = empty_dir(path);
	if (ret) {
		error("%s: empty_dir %s failed", __func__, path);
		goto out_sense;
	}
	
create:
	ret = osd_open(root, osd); /* will create files/dirs under root */
	if (ret != 0) {
		error("%s: osd_open %s failed", __func__, root);
		goto out_sense;
	}
	goto out;

out_sense:
	ret = sense_build_sdd(sense, OSD_SSK_HARDWARE_ERROR,
			      OSD_ASC_SYSTEM_RESOURCE_FAILURE, 0, 0);

out:
	free(root);
	return ret;
}


/*
 * TODO: test for sane values of: pid, oid, page, number
 */
int osd_get_attributes(struct osd_device *osd, uint64_t pid, uint64_t oid,
                       uint32_t page, uint32_t number, void *outbuf, 
		       uint16_t len, int getpage, uint8_t *sense)
{
	int ret = 0;

	debug("%s: get attr for (%llu, %llu)", __func__, llu(pid), llu(oid));
	
	if (outbuf == NULL || sense == NULL)
		goto out_build_sense;

	if (getpage == 0) {
		ret = attr_get_attr(osd->db, pid, oid, page, number, 
				    outbuf, len);
		if (ret != 0)
			goto out_build_sense;
	} else if (getpage == 1) {
		if (page == CUR_CMD_ATTR_PG) {
			/* 
			 * XXX: it is possible to read stale ccap, or ccap
			 * of another object 
			 */
			ret = get_ccap(osd, outbuf, len);
		} else {
			ret = attr_get_attr_page(osd->db, pid, oid, page, 
						 outbuf, len);
		}
		if (ret != 0) 
			goto out_build_sense;
	} else {
		goto out_build_sense;
	}
	return 0; /* success */

out_build_sense:
	ret = sense_build_sdd(sense, OSD_SSK_ILLEGAL_REQUEST,
			      OSD_ASC_INVALID_FIELD_IN_PARAMETER_LIST, 
			      pid, oid);
	return ret;
}


int osd_get_member_attributes(struct osd_device *osd, uint64_t pid,
			      uint64_t cid, uint8_t *sense)
{
	debug(__func__);
	return 0;
}


int osd_list(struct osd_device *osd, uint64_t pid, uint32_t list_id,
	     uint64_t alloc_len, uint64_t initial_oid, uint8_t *sense)
{
	debug(__func__);
	return 0;
}


int osd_list_collection(struct osd_device *osd, uint64_t pid, uint64_t cid,
                        uint32_t list_id, uint64_t alloc_len,
			uint64_t initial_oid, uint8_t *sense)
{
	debug(__func__);
	return 0;
}


int osd_query(struct osd_device *osd, uint64_t pid, uint64_t cid,
	      uint32_t query_len, uint64_t alloc_len, uint8_t *sense)
{
	debug(__func__);
	return 0;
}

/*
 * @offset: offset from byte zero of the object where data will be read
 * @len: length of data to be read
 * @doutbuf: pointer to pointer start of the data-out-buffer: destination 
 * 	of read
 */

int osd_read(struct osd_device *osd, uint64_t pid, uint64_t oid, uint64_t len,
	     uint64_t offset, uint8_t **doutbuf, uint64_t *outlen, 
	     uint8_t *sense)
{
	ssize_t retlen;
	int ret, fd;
	char path[MAXNAMELEN];
	void *buf = NULL;

	debug("%s: pid %llu oid %llu len %llu offset %llu", __func__,
	      llu(pid), llu(oid), llu(len), llu(offset));
	
	if (osd == NULL || osd->root == NULL || doutbuf == NULL || 
	    outlen == NULL)
		goto out_cdb_err;
	
	get_dfile_name(path, osd->root, pid, oid);
	fd = open(path, O_RDWR|O_LARGEFILE); /* fails on non-existent obj */
	if (fd < 0)
		goto out_cdb_err;

	buf = Malloc(len); /* freed in iSCSI layer */
	retlen = pread(fd, buf, len, offset);
	if (retlen < 0) {
		close(fd);
		goto out_hw_err;
	}

	ret = close(fd);
	if (ret != 0)
		goto out_hw_err;

	*outlen = retlen;
	*doutbuf = buf;

#if 0   /* Causes scsi transport problems.  Will debug later.  --pw */
	/* valid, but return a sense code */
	if (retlen < len) {
		ret = sense_build_sdd(sense, OSD_SSK_RECOVERED_ERROR,
		                      OSD_ASC_READ_PAST_END_OF_USER_OBJECT,
				      pid, oid);
		ret += sense_csi_build(sense+ret, MAX_SENSE_LEN-ret, retlen);
	}
#endif
	return ret; /* success */

out_hw_err:
	if (buf)
		free(buf); /* error, hence free here itself */
	ret = sense_build_sdd(sense, OSD_SSK_HARDWARE_ERROR,
			      OSD_ASC_INVALID_FIELD_IN_CDB, pid, oid);
	return ret;

out_cdb_err:
	if (buf)
		free(buf); /* error, hence free here itself */
	ret = sense_build_sdd(sense, OSD_SSK_ILLEGAL_REQUEST,
			      OSD_ASC_INVALID_FIELD_IN_CDB, pid, oid);
	return ret;
}


int osd_remove(struct osd_device *osd, uint64_t pid, uint64_t oid,
               uint8_t *sense)
{
	int ret = 0;
	char path[MAXNAMELEN];
	
	debug("%s: removing userobject pid %llu oid %llu", __func__,
	      llu(pid), llu(oid));

	get_dfile_name(path, osd->root, pid, oid);
	ret = unlink(path);
	if (ret != 0) 
		goto out_err;

	/* delete all attr of the object */
	ret = attr_delete_all(osd->db, pid, oid);
	if (ret != 0) 
		goto out_err;

	ret = obj_delete(osd->db, pid, oid);
	if (ret != 0)
		goto out_err;

	return 0; /* success */

out_err:
	ret = sense_build_sdd(sense, OSD_SSK_HARDWARE_ERROR,
			      OSD_ASC_INVALID_FIELD_IN_CDB, pid, oid);
	return ret;
}


int osd_remove_collection(struct osd_device *osd, uint64_t pid, uint64_t cid,
                          uint8_t *sense)
{
	debug(__func__);
	return 0;
}


int osd_remove_member_objects(struct osd_device *osd, uint64_t pid,
			      uint64_t cid, uint8_t *sense)
{
	debug(__func__);
	return 0;
}


int osd_remove_partition(struct osd_device *osd, uint64_t pid, uint8_t *sense)
{
	debug(__func__);
	return 0;
}


/*
 * Settable page numbers in any given U,P,C,R range are further restricted
 * by osd2r00 p 23:  >= 0x10000 && < 0x20000000.
 *
 * -	XXX: attr directory setting
 */
int osd_set_attributes(struct osd_device *osd, uint64_t pid, uint64_t oid,
                       uint32_t page, uint32_t number, const void *val,
		       uint16_t len, uint8_t *sense)
{
	int ret = 0;

	debug("%s: set attr on pid %llu oid %llu", __func__, llu(pid), 
	      llu(oid));

	if (obj_ispresent(osd->db, pid, oid) == 0) /* object not present! */
		goto out_cdb;

	/* 
	 * For each kind of object, check if the page is within bounds.
	 * This also traps attribute directory page assignments.
	 */
	if (pid == ROOT_PID && oid == ROOT_OID) {
		if (page < ROOT_PG_LB || page == GETALLATTR_PG ||
		    (page > ROOT_PG_UB && page < ANY_PG))
			goto out_param_list;
		else 
			goto test_number;
	} else if (pid >= PARTITION_PID_LB && oid == PARTITION_OID) {
		if (page < PARTITION_PG_LB || page == GETALLATTR_PG ||
		    (page > PARTITION_PG_UB && page < ANY_PG))
			goto out_param_list;
		else 
			goto test_number;
	} else if (pid >= OBJECT_PID_LB && oid >= OBJECT_OID_LB) {
		if ((COLLECTION_PG_LB <= page && page <=COLLECTION_PG_UB) ||
		    (page != GETALLATTR_PG && page >= ANY_PG)) {
			goto test_number;
		} else if ((USEROBJECT_PG_LB <= page && 
			    page <= USEROBJECT_PG_UB) ||
			   (page != GETALLATTR_PG && page >= ANY_PG)) {
			goto test_number;

		} else {
			goto out_param_list;
		}
	} else {
		goto out_cdb;
	}

test_number:
	if (number == ATTRNUM_UNMODIFIABLE)
		goto out_param_list;
	if (val == NULL)
		goto out_cdb;

	/* information page, make sure null terminated. osd2r00 7.1.2.2 */
	if (number == ATTRNUM_INFO) {
		int i;
		const uint8_t *s = val;

		if (len > 40)
			goto out_cdb;
		for (i=0; i<len; i++) {
			if (s[i] == 0) 	
				break;
		}
		if (i == len)
			goto out_cdb;
	}

	/* 
	 * len == 0 is equivalent to deleting the attr. osd2r00 4.7.4 second
	 * last paragraph. only attributes with non zero length are
	 * retrieveable 
	 */
	if (len == 0) {
		ret = attr_delete_attr(osd->db, pid, oid, page, number);
		if (ret != 0)
			goto out_cdb;
		return ret; /* success */
	}

	ret = attr_set_attr(osd->db, pid, oid, page, number, val, len);
	if (ret != 0) {
		ret = sense_build_sdd(sense, OSD_SSK_HARDWARE_ERROR,
				      OSD_ASC_INVALID_FIELD_IN_CDB, pid, oid);
		return ret;
	}
	return ret; /* success */
	
out_param_list:
	ret = sense_build_sdd(sense, OSD_SSK_ILLEGAL_REQUEST,
			      OSD_ASC_INVALID_FIELD_IN_PARAMETER_LIST, 
			      pid, oid);
	return ret;

out_cdb:
	ret = sense_build_sdd(sense, OSD_SSK_ILLEGAL_REQUEST,
			      OSD_ASC_INVALID_FIELD_IN_CDB, pid, oid);
	return ret;
}


int osd_set_key(struct osd_device *osd, int key_to_set, uint64_t pid,
		uint64_t key, uint8_t seed[20], uint8_t *sense)
{
	debug(__func__);
	return 0;
}


int osd_set_master_key(struct osd_device *osd, int dh_step, uint64_t key,
                       uint32_t param_len, uint32_t alloc_len, uint8_t *sense)
{
	debug(__func__);
	return 0;
}


int osd_set_member_attributes(struct osd_device *osd, uint64_t pid,
			      uint64_t cid, uint8_t *sense)
{
	debug(__func__);
	return 0;
}


/*
 * TODO: We did not implement length as an attribute in attr table, hence
 * relying on underlying ext3 fs to get length of the object. Also since we
 * are not implementing quotas, we ignore maximum length attribute of an
 * object and the partition which stores the object.
 * 
 * @offset: offset from byte zero of the object where data will be written
 * @len: length of data to be written
 * @dinbuf: pointer to start of the Data-in-buffer: source of data
 */
int osd_write(struct osd_device *osd, uint64_t pid, uint64_t oid, uint64_t len,
	      uint64_t offset, const uint8_t *dinbuf, uint8_t *sense)
{
	int ret = 0;
	int fd = 0;
	char path[MAXNAMELEN];

	debug("%s: pid %llu oid %llu len %llu offset %llu data %p", __func__,
	      llu(pid), llu(oid), llu(len), llu(offset), dinbuf);

	if (osd == NULL || osd->root == NULL || dinbuf == NULL)
		goto out_cdb_err;

	get_dfile_name(path, osd->root, pid, oid);
	fd = open(path, O_RDWR|O_LARGEFILE); /* fails on non-existent obj */
	if (fd < 0)
		goto out_cdb_err; 

	ret = pwrite(fd, dinbuf, len, offset);
	if (ret < 0 || (uint64_t)ret != len)
		goto out_hw_err;

	ret = close(fd);
	if (ret != 0)
		goto out_hw_err;

	return 0; /* success */

out_hw_err:
	ret = sense_build_sdd(sense, OSD_SSK_HARDWARE_ERROR,
			      OSD_ASC_INVALID_FIELD_IN_CDB, pid, oid);
	return ret;

out_cdb_err:
	ret = sense_build_sdd(sense, OSD_SSK_ILLEGAL_REQUEST,
			      OSD_ASC_INVALID_FIELD_IN_CDB, pid, oid);
	return ret;
}

