#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "osd.h"
#include "db.h"
#include "attr.h"
#include "util.h"
#include "obj.h"
#include "osd-sense.h"

static const char *dbname = "osd.db";

/*
 * Module interface
 */
int osd_open(const char *root, struct osd_device *osd)
{
	int ret;
	char path[MAXNAMELEN];
	struct stat sb;

	if (strlen(root) > MAXROOTLEN) {
		ret = -ENAMETOOLONG;
		goto out;
	}

	/* test if root exists and is a directory */
	ret = stat(root, &sb);
	if (ret == 0) {
		if (!S_ISDIR(sb.st_mode)) {
			fprintf(stderr, "%s: root %s not a directory\n",
			        __func__, root);
			ret = -ENOTDIR;
			goto out;
		}
	} else {
		if (errno != ENOENT) {
			fprintf(stderr, "%s: stat root %s: %m\n",
			        __func__, root);
			ret = -ENOTDIR;
			goto out;
		}

		/* if not, create it */
		ret = mkdir(root, 0777);
		if (ret < 0) {
			fprintf(stderr, "%s: create root %s: %m\n",
			        __func__, root);
			goto out;
		}
	}

	/* auto-creates db if necessary, and sets osd->db */
	osd->root = strdup(root);
	sprintf(path, "%s/%s", root, dbname);
	ret = db_open(path, osd);

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
 * Descriptor format sense data.  See spc3 p 31.  Returns length of
 * buffer used so far.
 */
static int sense_header_build(uint8_t *data, int len, uint8_t key, uint8_t asc,
                              uint8_t ascq, uint8_t additional_len)
{
	if (len < 8)
		return 0;
	data[0] = 0x72;  /* current, not deferred */
	data[1] = key;
	data[2] = asc;
	data[3] = ascq;
	data[7] = additional_len;  /* additional length, beyond these 8 bytes */
	return 8;
}

/*
 * OSD object identification sense data descriptor.  Always need one
 * of these, then perhaps some others.  This goes just beyond the 8
 * byte sense header above.  The length of this is 32.
 */
static int sense_info_build(uint8_t *data, int len, uint32_t not_init_funcs,
                            uint32_t completed_funcs, uint64_t pid,
			    uint64_t oid)
{
	if (len < 32)
		return 0;
	data[0] = 0x6;
	data[1] = 0x1e;
	/* XXX: get interface split up properly...
	htonl_set(&data[8], not_init_funcs);
	htonl_set(&data[12], completed_funcs);
	htonll_set(&data[16], pid);
	htonll_set(&data[24], oid);
	*/
	return 32;
}

/*
 * Helper to create sense data where no processing was initiated or completed,
 * and just a header and basic info descriptor are required.  Assumes full 252
 * byte sense buffer.
 */
static int sense_basic_build(uint8_t *sense, uint8_t key, uint8_t asc,
                             uint8_t ascq, uint64_t pid, uint64_t oid)
{
	uint8_t off = 0;
	uint8_t len = MAX_SENSE_LEN;
	uint32_t nifunc = 0x303010b0;  /* non-reserved bits */

	off = sense_header_build(sense+off, len-off, key, asc, ascq, 40);
	off = sense_info_build(sense+off, len-off, nifunc, 0, pid, oid);
	return off;
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

	sprintf(path, "%s/%llu.%llu", osd->root, llu(pid), llu(oid));
	ret = stat(path, &sb);
	if (ret == 0 && S_ISREG(sb.st_mode)) {
		return -EEXIST;
	} else if (ret == -1 && errno == ENOENT) {
		ret = creat(path, 0666);
		if (ret <= 0)
			return ret;
		close(ret);
	}

	return 0;
}

/*
 * -	check if requested oid and pid are in legal range
 * -	if pid == 0 return error
 * -	if oid == 0 assign any oid
 * -	if oid != 0 and oid assignment fails return err
 * -	unique oid for user objects and collections within a partition_info
 * -	if num == 0 || 1, create one object, else create num user objects.
 * 	assign consequetive values to oids. place the lowest valued oid in
 * 	current command attribute.
 * -	if num > 1 and requested oid != 0 return error
 * FIXME: get set attributes left
 */
int osd_create(struct osd_device *osd, uint64_t pid, uint64_t requested_oid,
	       uint16_t num, uint8_t *sense)
{
	int i, ret;
	uint64_t oid = 0;

	debug("%s: pid %llu requested oid %llu num %hu", __func__, llu(pid),
	      llu(requested_oid), num);

	if (pid == 0 || pid < USEROBJECT_PID_LB) {
		ret = sense_basic_build(sense, OSD_SSK_ILLEGAL_REQUEST, 
					ASC(OSD_ASC_INVALID_FIELD_IN_CDB), 
					ASCQ(OSD_ASC_INVALID_FIELD_IN_CDB),
					pid, requested_oid);
		return ret;
	}
	
	if (requested_oid != 0 && requested_oid < USEROBJECT_OID_LB) {
		ret = sense_basic_build(sense, OSD_SSK_ILLEGAL_REQUEST, 
					ASC(OSD_ASC_INVALID_FIELD_IN_CDB), 
					ASCQ(OSD_ASC_INVALID_FIELD_IN_CDB),
					pid, requested_oid);
		return ret;
	}
	
	if (num > 1 && requested_oid != 0) {
		ret = sense_basic_build(sense, OSD_SSK_ILLEGAL_REQUEST, 
					ASC(OSD_ASC_INVALID_FIELD_IN_CDB), 
					ASCQ(OSD_ASC_INVALID_FIELD_IN_CDB),
					pid, requested_oid);
		return ret;
	}

	if (requested_oid == 0) {
		ret = obj_get_nextoid(osd->db, pid, USEROBJECT, &oid);
		if (ret != 0) {
			ret = sense_basic_build(sense, OSD_SSK_HARDWARE_ERROR, 
						ASC(OSD_ASC_INVALID_FIELD_IN_CDB), 
						ASCQ(OSD_ASC_INVALID_FIELD_IN_CDB),
						pid, requested_oid);
			return ret;
		}
	} else {
		ret = obj_ispresent(osd->db, pid, requested_oid);
		if (ret != 0) {
			ret = sense_basic_build(sense, OSD_SSK_HARDWARE_ERROR, 
						ASC(OSD_ASC_INVALID_FIELD_IN_CDB), 
						ASCQ(OSD_ASC_INVALID_FIELD_IN_CDB),
						pid, requested_oid);
			return ret;
		}
		oid = requested_oid; /* requested_oid works! */
	}

	if (num == 0)
		num = 1; /* create atleast one object */
	for (i=0; i<num; i++) {
		ret = obj_insert(osd->db, pid, oid+i, USEROBJECT);
		if (ret != 0) {
			ret = sense_basic_build(sense, OSD_SSK_HARDWARE_ERROR, 
						ASC(OSD_ASC_INVALID_FIELD_IN_CDB), 
						ASCQ(OSD_ASC_INVALID_FIELD_IN_CDB),
						pid, oid);
			return ret;
		}
		ret = osd_create_datafile(osd, pid, oid+i);
		if (ret) {
			obj_delete(osd->db, pid, oid+i); /* delete new obj */
			ret = sense_basic_build(sense, OSD_SSK_HARDWARE_ERROR, 
						ASC(OSD_ASC_INVALID_FIELD_IN_CDB), 
						ASCQ(OSD_ASC_INVALID_FIELD_IN_CDB),
						pid, oid);
			return ret;
		}
	}

	return 0;
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
	int ret;

	debug(__func__);
	ret = create_partition(osd, 0);
	/* XXX: generate sense data from bad ret sometime */
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
	
	debug("%s: capacity %llu MB", __func__, llu(capacity >> 20));

	root = strdup(osd->root);
	ret = db_close(osd);
	if (ret)
		goto out;
	sprintf(path, "%s/%s", root, dbname);
	ret = unlink(path);
	if (ret) {
		error_errno("%s: unlink db %s", __func__, path);
		goto out;
	}
	ret = osd_open(root, osd);
	free(root);
out:
	return ret;
}


int osd_get_attributes(struct osd_device *osd, uint64_t pid, uint64_t oid,
                       uint8_t *sense)
{
	debug(__func__);
	return 0;
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


int osd_read(struct osd_device *osd, uint64_t pid, uint64_t oid, uint64_t len,
	     uint64_t offset, uint8_t **data, uint64_t *outlen, uint8_t *sense)
{
	const char result[] = "Falls mainly in the plain";
	int ret;
	uint8_t *v;

	debug("%s: pid %llu oid %llu len %llu offset %llu", __func__,
	      llu(pid), llu(oid), llu(len), llu(offset));
	if (pid == 16) {
		/* Testing read of non-existent object.  Return the appropriate
		 * error.
		 */
		ret = sense_basic_build(sense, OSD_SSK_ILLEGAL_REQUEST, 0, 0,
		                        pid, oid);
		return ret;
	}

	v = malloc(len);
	if (!v) {
		error("%s: malloc %llu bytes", __func__, llu(len));
		/* actually -ENOMEM, but do not know what better to return */
		ret = sense_basic_build(sense, OSD_SSK_ILLEGAL_REQUEST, 0, 0,
		                        pid, oid);
		return ret;
	}
	memset(v, 0, len);
	if (len > strlen(result))
		len = strlen(result);
	memcpy(v, result, len);
	v[len-1] = '\0';
	*data = v;
	*outlen = len;
	return 0;
}


int osd_remove(struct osd_device *osd, uint64_t pid, uint64_t oid,
               uint8_t *sense)
{
	int ret = 0;
	char path[MAXNAMELEN];
	
	debug("%s: removing userobject pid %llu oid %llu", __func__,
	      llu(pid), llu(oid));

	sprintf(path, "%s/%llu.%llu", osd->root, llu(pid), llu(oid));
	ret = unlink(path);
	if (ret != 0) {
		ret = sense_basic_build(sense, OSD_SSK_HARDWARE_ERROR, 
					ASC(OSD_ASC_INVALID_FIELD_IN_CDB), 
					ASCQ(OSD_ASC_INVALID_FIELD_IN_CDB),
					pid, oid);
		return ret;
	}

	ret = obj_delete(osd->db, pid, oid);
	if (ret != 0) {
		ret = sense_basic_build(sense, OSD_SSK_ILLEGAL_REQUEST, 
					ASC(OSD_ASC_INVALID_FIELD_IN_CDB), 
					ASCQ(OSD_ASC_INVALID_FIELD_IN_CDB),
					pid, oid);
		return ret;
	}

	return 0;
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
 */
int osd_set_attributes(struct osd_device *osd, uint64_t pid, uint64_t oid,
                       uint8_t *sense)
{
	debug(__func__);
	return 0;
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


int osd_write(struct osd_device *osd, uint64_t pid, uint64_t oid, uint64_t len,
	      uint64_t offset, const uint8_t *data, uint8_t *sense)
{
	debug("%s: pid %llu oid %llu len %llu offset %llu data %p", __func__,
	      llu(pid), llu(oid), llu(len), llu(offset), data);
	return 0;
}

