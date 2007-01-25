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
 * Commands
 */
int osd_append(struct osd_device *osd, uint64_t pid, uint64_t oid, uint64_t len,
               uint8_t *data)
{
	debug(__func__);
	return 0;
}


/*
 * Following steps for creating objects
 * -	check if requested_oid is in user
 */
int osd_create(struct osd_device *osd, uint64_t pid, uint64_t requested_oid, 
	       uint16_t num, )
{
	int i, ret;

	debug("%s: pid %llu requested oid %llu num %hu", __func__, llu(pid),
	      llu(requested_oid), num);
	if (pid == 0)
		return -1;

	for (i=0; i<num; i++) {
		ret = obj_insert(osd->db, pid, requested_oid);
		if (ret)
			break;
	}
	return ret;
}


int osd_create_and_write(struct osd_device *osd, uint64_t pid, 
			 uint64_t requested_oid, uint64_t len, uint64_t offset,
			 uint8_t *data)
{
	debug(__func__);
	return 0;
}


int osd_create_collection(struct osd_device *osd, uint64_t pid, 
			  uint64_t requested_cid)
{
	debug(__func__);
	return 0;
}

static struct init_attr partition_info[] = {
	{ PARTITION_PG + 0, 0, "INCITS  T10 Partition Directory" },
	{ PARTITION_PG + 0, PARTITION_PG + 1,
		"INCITS  T10 Partition Information" },
	{ PARTITION_PG + 1, 0, "INCITS  T10 Partition Information" },
};

int osd_create_partition(struct osd_device *osd, uint64_t requested_pid)
{
	int i, ret;

	if (requested_pid != 0) {
		/*
		 * Partition zero does not have an entry in the obj db; those
		 * are only for user-created partitions.
		 */
		ret = obj_insert(osd->db, requested_pid, 0);
		if (ret)
			goto out;
	}
	for (i=0; i<ARRAY_SIZE(partition_info); i++) {
		struct init_attr *ia = &partition_info[i];
		ret = attr_set_attr(osd->db, requested_pid, 0, ia->page, 
				    ia->number, ia->s, strlen(ia->s)+1);
		if (ret)
			goto out;
	}

	/* the pid goes here */
	ret = attr_set_attr(osd->db, requested_pid, 0, PARTITION_PG + 1, 
			    1, &requested_pid, 8);
	if (ret)
		goto out;

out:
	return ret;
}


int osd_flush(struct osd_device *osd, uint64_t pid, uint64_t oid, 
	      int flush_scope)
{
	debug(__func__);
	return 0;
}


int osd_flush_collection(struct osd_device *osd, uint64_t pid, uint64_t cid,
                         int flush_scope)
{
	debug(__func__);
	return 0;
}


int osd_flush_osd(struct osd_device *osd, int flush_scope)
{
	debug(__func__);
	return 0;
}


int osd_flush_partition(struct osd_device *osd, uint64_t pid, int flush_scope)
{
	debug(__func__);
	return 0;
}

/*
 * Destroy the db and start over again.
 */
int osd_format_osd(struct osd_device *osd, uint64_t capacity)
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


int osd_get_attributes(struct osd_device *osd, uint64_t pid, uint64_t oid)
{
	debug(__func__);
	return 0;
}


int osd_get_member_attributes(struct osd_device *osd, uint64_t pid, 
			      uint64_t cid)
{
	debug(__func__);
	return 0;
}


int osd_list(struct osd_device *osd, uint64_t pid, uint32_t list_id, 
	     uint64_t alloc_len, uint64_t initial_oid)
{
	debug(__func__);
	return 0;
}


int osd_list_collection(struct osd_device *osd, uint64_t pid, uint64_t cid,
                        uint32_t list_id, uint64_t alloc_len,
			uint64_t initial_oid)
{
	debug(__func__);
	return 0;
}


int osd_query(struct osd_device *osd, uint64_t pid, uint64_t cid, 
	      uint32_t query_len, uint64_t alloc_len)
{
	debug(__func__);
	return 0;
}


int osd_read(struct osd_device *osd, uint64_t pid, uint64_t oid, uint64_t len,
	     uint64_t offset, uint8_t **data, uint64_t *outlen)
{
	const char result[] = "Falls mainly in the plain";
	uint8_t *v;

	debug("%s: pid %llu oid %llu len %llu offset %llu", __func__,
	      llu(pid), llu(oid), llu(len), llu(offset));
	v = malloc(len);
	if (!v) {
		error("%s: malloc %llu bytes", __func__, llu(len));
		return -ENOMEM;
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


int osd_remove(struct osd_device *osd, uint64_t pid, uint64_t uid)
{
	debug(__func__);
	return 0;
}


int osd_remove_collection(struct osd_device *osd, uint64_t pid, uint64_t cid)
{
	debug(__func__);
	return 0;
}


int osd_remove_member_objects(struct osd_device *osd, uint64_t pid, 
			      uint64_t cid)
{
	debug(__func__);
	return 0;
}


int osd_remove_partition(struct osd_device *osd, uint64_t pid)
{
	debug(__func__);
	return 0;
}


/*
 * Settable page numbers in any given U,P,C,R range are further restricted
 * by osd2r00 p 23:  >= 0x10000 && < 0x20000000.
 */
int osd_set_attributes(struct osd_device *osd, uint64_t pid, uint64_t oid)
{
	debug(__func__);
	return 0;
}


int osd_set_key(struct osd_device *osd, int key_to_set, uint64_t pid, 
		uint64_t key, uint8_t seed[20])
{
	debug(__func__);
	return 0;
}


int osd_set_master_key(struct osd_device *osd, int dh_step, uint64_t key,
                       uint32_t param_len, uint32_t alloc_len)
{
	debug(__func__);
	return 0;
}


int osd_set_member_attributes(struct osd_device *osd, uint64_t pid, 
			      uint64_t cid)
{
	debug(__func__);
	return 0;
}


int osd_write(struct osd_device *osd, uint64_t pid, uint64_t oid, uint64_t len,
	      uint64_t offset, const uint8_t *data)
{
	debug("%s: pid %llu oid %llu len %llu offset %llu data %p", __func__,
	      llu(pid), llu(oid), llu(len), llu(offset), data);
	return 0;
}

