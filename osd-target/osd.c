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

const char *dbname = "osd.db";

/*
 * Module interface
 */
int osd_open(const char *root, osd_t *osd)
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
	if (ret < 0)
		goto out;

	ret = obj_insert(osd, 0, 0);
	if (ret == EEXIST)
		ret = 0;
out:
	return ret;
}

int osd_close(osd_t *osd)
{
	int ret;
	
	ret = db_close(osd);
	free(osd->root);
	return ret;
}

/*
 * Commands
 */
int osd_append(osd_t *osd, uint64_t pid, uint64_t oid, uint64_t len)
{
	debug(__func__);
	return 0;
}


int osd_create(osd_t *osd, uint64_t pid, uint64_t requested_oid, uint16_t num)
{
	debug(__func__);
	return 0;
}


int osd_create_and_write(osd_t *osd, uint64_t pid, uint64_t requested_oid,
                         uint64_t len, uint64_t offset)
{
	debug(__func__);
	return 0;
}


int osd_create_collection(osd_t *osd, uint64_t pid, uint64_t requested_cid)
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

int osd_create_partition(osd_t *osd, uint64_t requested_pid)
{
	int i, ret;

	if (requested_pid != 0) {
		/*
		 * Partition zero does not have an entry in the obj db; those
		 * are only for user-created partitions.
		 */
		ret = obj_insert(osd, requested_pid, 0);
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


int osd_flush(osd_t *osd, uint64_t pid, uint64_t oid, int flush_scope)
{
	debug(__func__);
	return 0;
}


int osd_flush_collection(osd_t *osd, uint64_t pid, uint64_t cid,
                         int flush_scope)
{
	debug(__func__);
	return 0;
}


int osd_flush_osd(osd_t *osd, int flush_scope)
{
	debug(__func__);
	return 0;
}


int osd_flush_partition(osd_t *osd, uint64_t pid, int flush_scope)
{
	debug(__func__);
	return 0;
}

/*
 * Destroy the db and start over again.
 */
int osd_format(osd_t *osd, uint64_t capacity)
{
	int ret;
	char *root;
	char path[MAXNAMELEN];
	
	debug(__func__);

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


int osd_get_attributes(osd_t *osd, uint64_t pid, uint64_t oid)
{
	debug(__func__);
	return 0;
}


int osd_get_member_attributes(osd_t *osd, uint64_t pid, uint64_t cid)
{
	debug(__func__);
	return 0;
}


int osd_list(osd_t *osd, uint64_t pid, uint32_t list_id, uint64_t alloc_len,
             uint64_t initial_oid)
{
	debug(__func__);
	return 0;
}


int osd_list_collection(osd_t *osd, uint64_t pid, uint64_t cid,
                        uint32_t list_id, uint64_t alloc_len,
			uint64_t initial_oid)
{
	debug(__func__);
	return 0;
}


int osd_query(osd_t *osd, uint64_t pid, uint64_t cid, uint32_t query_len,
              uint64_t alloc_len)
{
	debug(__func__);
	return 0;
}


int osd_read(osd_t *osd, uint64_t pid, uint64_t uid, uint64_t len,
	     uint64_t offset)
{
	debug(__func__);
	return 0;
}


int osd_remove(osd_t *osd, uint64_t pid, uint64_t uid)
{
	debug(__func__);
	return 0;
}


int osd_remove_collection(osd_t *osd, uint64_t pid, uint64_t cid)
{
	debug(__func__);
	return 0;
}


int osd_remove_member_objects(osd_t *osd, uint64_t pid, uint64_t cid)
{
	debug(__func__);
	return 0;
}


int osd_remove_partition(osd_t *osd, uint64_t pid)
{
	debug(__func__);
	return 0;
}


/*
 * Settable page numbers in any given U,P,C,R range are further restricted
 * by osd2r00 p 23:  >= 0x10000 && < 0x20000000.
 */
int osd_set_attributes(osd_t *osd, uint64_t pid, uint64_t oid)
{
	debug(__func__);
	return 0;
}


int osd_set_key(osd_t *osd, int key_to_set, uint64_t pid, uint64_t key,
                uint8_t seed[20])
{
	debug(__func__);
	return 0;
}


int osd_set_master_key(osd_t *osd, int dh_step, uint64_t key,
                       uint32_t param_len, uint32_t alloc_len)
{
	debug(__func__);
	return 0;
}


int osd_set_member_attributes(osd_t *osd, uint64_t pid, uint64_t cid)
{
	debug(__func__);
	return 0;
}


int osd_write(osd_t *osd, uint64_t pid, uint64_t uid, uint64_t len,
	      uint64_t offset)
{
	debug(__func__);
	return 0;
}

