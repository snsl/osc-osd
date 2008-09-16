/*
 * Initial command descriptor block parsing.  Gateway into the
 * core osd functions in osd.c.
 */
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "osd.h"
#include "osd-sense.h"
#include "osd-defs.h"
#include "cdb.h"
#include "util/util.h"


/*
 * Compare, and complain if not the same.  Return sense data if so.
 */
static int verify_enough_input_data(uint64_t datalen, uint64_t cdblen,
                                    uint8_t *sense)
{
	int ret = 0;

	if (cdblen != datalen) {
		error("%s: supplied data %llu expected %llu\n",
		      __func__, llu(datalen), llu(cdblen));
		ret = osd_error_bad_cdb(sense);
	}
	return ret;
}

/*
 * uaddr: either input or output, depending on the command
 * uaddrlen: output var with number of valid bytes put in uaddr after a read
 */
int osdemu_cmd_submit(struct osd_device *osd, uint8_t *cdb,
                         uint8_t *data_in, uint64_t data_in_len,
		         uint8_t **data_out, uint64_t *data_out_len,
		         uint8_t **sense_out, uint8_t *senselen_out)
{
	int ret = -EINVAL;
	uint16_t action = (cdb[8] << 8) | cdb[9];
	uint8_t sense[MAX_SENSE_LEN];  /* for output sense data */

	memset(sense, 0, MAX_SENSE_LEN);

	switch (action) {
	case OSD_APPEND: {
		uint64_t pid = ntohll(&cdb[16]);
		uint64_t oid = ntohll(&cdb[24]);
		uint64_t len = ntohll(&cdb[36]);
		ret = verify_enough_input_data(data_in_len, len, sense);
		if (ret)
			break;
		ret = osd_append(osd, pid, oid, len, data_in, sense);
		break;
	}
	case OSD_CREATE: {
		uint64_t pid = ntohll(&cdb[16]);
		uint64_t requested_oid = ntohll(&cdb[24]);
		uint16_t num = ntohs(&cdb[36]);
		ret = osd_create(osd, pid, requested_oid, num, sense);
		break;
	}
	case OSD_CREATE_AND_WRITE: {
		uint64_t pid = ntohll(&cdb[16]);
		uint64_t requested_oid = ntohll(&cdb[24]);
		uint64_t len = ntohll(&cdb[36]);
		uint64_t offset = ntohs(&cdb[44]);
		ret = verify_enough_input_data(data_in_len, len, sense);
		if (ret)
			break;
		ret = osd_create_and_write(osd, pid, requested_oid, len,
		                           offset, data_in, sense);
		break;
	}
	case OSD_CREATE_COLLECTION: {
		uint64_t pid = ntohll(&cdb[16]);
		uint64_t requested_cid = ntohll(&cdb[24]);
		ret = osd_create_collection(osd, pid, requested_cid, sense);
		break;
	}
	case OSD_CREATE_PARTITION: {
		uint64_t requested_pid = ntohll(&cdb[16]);
		ret = osd_create_partition(osd, requested_pid, sense);
		break;
	}
	case OSD_FLUSH: {
		uint64_t pid = ntohll(&cdb[16]);
		uint64_t oid = ntohll(&cdb[24]);
		int flush_scope = cdb[10] & 0x3;
		ret = osd_flush(osd, pid, oid, flush_scope, sense);
		break;
	}
	case OSD_FLUSH_COLLECTION: {
		uint64_t pid = ntohll(&cdb[16]);
		uint64_t cid = ntohll(&cdb[24]);
		int flush_scope = cdb[10] & 0x3;
		ret = osd_flush_collection(osd, pid, cid, flush_scope, sense);
		break;
	}
	case OSD_FLUSH_OSD: {
		int flush_scope = cdb[10] & 0x3;
		ret = osd_flush_osd(osd, flush_scope, sense);
		break;
	}
	case OSD_FLUSH_PARTITION: {
		uint64_t pid = ntohll(&cdb[16]);
		int flush_scope = cdb[10] & 0x3;
		ret = osd_flush_partition(osd, pid, flush_scope, sense);
		break;
	}
	case OSD_FORMAT_OSD: {
		uint64_t capacity = ntohll(&cdb[36]);
		ret = osd_format_osd(osd, capacity, sense);
		break;
	}
	case OSD_GET_ATTRIBUTES: {
		uint64_t pid = ntohll(&cdb[16]);
		uint64_t oid = ntohll(&cdb[24]);
		ret = osd_get_attributes(osd, pid, oid, sense);
		break;
	}
	case OSD_GET_MEMBER_ATTRIBUTES: {
		uint64_t pid = ntohll(&cdb[16]);
		uint64_t cid = ntohll(&cdb[24]);
		ret = osd_get_member_attributes(osd, pid, cid, sense);
		break;
	}
	case OSD_LIST: {
		uint64_t pid = ntohll(&cdb[16]);
		uint32_t list_id = ntohl(&cdb[32]);
		uint64_t alloc_len = ntohll(&cdb[36]);
		uint64_t initial_oid = ntohll(&cdb[44]);
		ret = osd_list(osd, pid, list_id, alloc_len, initial_oid,
		               sense);
		break;
	}
	case OSD_LIST_COLLECTION: {
		uint64_t pid = ntohll(&cdb[16]);
		uint64_t cid = ntohll(&cdb[24]);
		uint32_t list_id = ntohl(&cdb[32]);
		uint64_t alloc_len = ntohll(&cdb[36]);
		uint64_t initial_oid = ntohll(&cdb[44]);
		ret = osd_list_collection(osd, pid, cid, list_id, alloc_len,
		                          initial_oid, sense);
		break;
	}
	case OSD_PERFORM_SCSI_COMMAND:
	case OSD_PERFORM_TASK_MGMT_FUNC:
		ret = osd_error_unimplemented(action, sense);
		break;
	case OSD_QUERY: {
		uint64_t pid = ntohll(&cdb[16]);
		uint64_t cid = ntohll(&cdb[24]);
		uint32_t query_len = ntohl(&cdb[32]);
		uint64_t alloc_len = ntohll(&cdb[36]);
		ret = osd_query(osd, pid, cid, query_len, alloc_len, sense);
		break;
	}
	case OSD_READ: {
		uint64_t pid = ntohll(&cdb[16]);
		uint64_t oid = ntohll(&cdb[24]);
		uint64_t len = ntohll(&cdb[36]);
		uint64_t offset = ntohll(&cdb[44]);
		ret = osd_read(osd, pid, oid, len, offset, data_out,
		               data_out_len, sense);
		break;
	}
	case OSD_REMOVE: {
		uint64_t pid = ntohll(&cdb[16]);
		uint64_t oid = ntohll(&cdb[24]);
		ret = osd_remove(osd, pid, oid, sense);
		break;
	}
	case OSD_REMOVE_COLLECTION: {
		uint64_t pid = ntohll(&cdb[16]);
		uint64_t cid = ntohll(&cdb[24]);
		ret = osd_remove_collection(osd, pid, cid, sense);
		break;
	}
	case OSD_REMOVE_MEMBER_OBJECTS: {
		uint64_t pid = ntohll(&cdb[16]);
		uint64_t cid = ntohll(&cdb[24]);
		ret = osd_remove_member_objects(osd, pid, cid, sense);
		break;
	}
	case OSD_REMOVE_PARTITION: {
		uint64_t pid = ntohll(&cdb[16]);
		ret = osd_remove_partition(osd, pid, sense);
		break;
	}
	case OSD_SET_ATTRIBUTES: {
		uint64_t pid = ntohll(&cdb[16]);
		uint64_t oid = ntohll(&cdb[24]);
		uint32_t page = 0; /* XXX: fillme */
		uint32_t number = 0; /* XXX: fillme */
		void *val = NULL; /* XXX: fillme */
		uint16_t len = 0; /* XXX: fillme */
		ret = osd_set_attributes(osd, pid, oid, page, number, val,
					 len, sense);
		break;
	}
	case OSD_SET_KEY: {
		int key_to_set = cdb[11] & 0x3;
		uint64_t pid = ntohll(&cdb[16]);
		uint64_t key = ntohll(&cdb[24]);
		uint8_t seed[20];
		memcpy(seed, &cdb[32], 20);
		ret = osd_set_key(osd, key_to_set, pid, key, seed, sense);
		break;
	}
	case OSD_SET_MASTER_KEY: {
		int dh_step = cdb[11] & 0x3;
		uint64_t key = ntohll(&cdb[24]);
		uint32_t param_len = ntohl(&cdb[32]);
		uint32_t alloc_len = ntohl(&cdb[36]);
		ret = osd_set_master_key(osd, dh_step, key, param_len,
		                         alloc_len, sense);
		break;
	}
	case OSD_SET_MEMBER_ATTRIBUTES: {
		uint64_t pid = ntohll(&cdb[16]);
		uint64_t cid = ntohll(&cdb[24]);
		ret = osd_set_member_attributes(osd, pid, cid, sense);
		break;
	}
	case OSD_WRITE: {
		uint64_t pid = ntohll(&cdb[16]);
		uint64_t oid = ntohll(&cdb[24]);
		uint64_t len = ntohll(&cdb[36]);
		uint64_t offset = ntohll(&cdb[44]);
		ret = verify_enough_input_data(data_in_len, len, sense);
		if (ret)
			break;
		ret = osd_write(osd, pid, oid, len, offset, data_in, sense);
		break;
	}
	default:
		ret = osd_error_unimplemented(action, sense);
	}

	if (ret < 0) {
		error("%s: ret should never be negative here", __func__);
		ret = 0;
	} else if (ret == 0) {
		ret = SAM_STAT_GOOD;
	} else {
		/* valid sense data, length is ret, maybe good data too */
		*sense_out = malloc(MAX_SENSE_LEN);
		if (*sense_out) {
			*senselen_out = ret;
			memcpy(*sense_out, sense, *senselen_out);
		}
		ret = SAM_STAT_CHECK_CONDITION;
	}
	/* return a SAM_STAT upwards */
	return ret;
}

