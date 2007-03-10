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
#include "util/osd-defs.h"
#include "target-defs.h"
#include "cdb.h"
#include "util/util.h"

/*
 * Aggregate parameters for function calls in this file.
 */
struct command {
	struct osd_device *osd;
	uint8_t *cdb;
	uint16_t action;
	uint8_t getset_cdbfmt;
	uint64_t retrieved_attr_off;
	const uint8_t *indata;
	uint64_t inlen;
	uint8_t *outdata;
	uint64_t outlen;
	uint64_t used_outlen;
	uint32_t get_used_outlen;
	uint8_t sense[MAX_SENSE_LEN];
	int senselen;
};

/*
 * Compare, and complain if not the same.  Return sense data if so.
 */
static int verify_enough_input_data(struct command *cmd, uint64_t cdblen)
{
	int ret = 0;

	if (cdblen != cmd->inlen) {
		osd_error("%s: supplied data %llu but cdb says %llu\n",
			  __func__, llu(cmd->inlen), llu(cdblen));
		ret = osd_error_bad_cdb(cmd->sense);
	}
	return ret;
}

/*
 * What's the total number of bytes this command might produce?
 */
static int calc_max_out_len(struct command *cmd)
{
	uint64_t end = 0;

	/* These commands return data. */
	switch (cmd->action) {
	case OSD_LIST:
	case OSD_LIST_COLLECTION:
	case OSD_READ:
	case OSD_SET_MASTER_KEY:
		cmd->outlen = ntohll(&cmd->cdb[36]);
		break;
	default:
		cmd->outlen = 0;
	}

	/*
	 * All commands might have getattr requests.  Add together
	 * the result offset and the allocation length to get the
	 * end of their write.
	 */
	cmd->retrieved_attr_off = 0;
	if (cmd->getset_cdbfmt == GETPAGE_SETVALUE) {
		cmd->retrieved_attr_off = ntohoffset(&cmd->cdb[60]);
		end = cmd->retrieved_attr_off + ntohl(&cmd->cdb[56]);
	} else if (cmd->getset_cdbfmt == GETLIST_SETLIST) {
		cmd->retrieved_attr_off = ntohoffset(&cmd->cdb[64]);
		end = cmd->retrieved_attr_off + ntohl(&cmd->cdb[60]);
	} else {
		return -1; /* TODO: proper error code */
	}

	if (end > cmd->outlen)
		cmd->outlen = end;

	return 0 /* TODO: proper error code */;
}

static int get_attr_page(struct command *cmd, uint64_t pid, uint64_t oid,
			 uint8_t isembedded, uint16_t numoid)
{
	int ret = 0;
	uint8_t *cdb = cmd->cdb;
	uint32_t page = ntohl(&cmd->cdb[52]);
	uint32_t alloc_len = ntohl(&cdb[56]); 
	void *outbuf = NULL;
	

	if (page == 0 || alloc_len == 0)/* nothing to be retrieved. osd2r00 */
		return 0;	   	/* Sec 5.2.2.2 */ 

	if (numoid > 1 && page != CUR_CMD_ATTR_PG)
		goto out_param_list_err;

	if (!cmd->outdata)
		goto out_param_list_err;

	outbuf = &cmd->outdata[cmd->retrieved_attr_off];
	return osd_getattr_page(cmd->osd, pid, oid, page, outbuf, alloc_len,
				isembedded, &cmd->get_used_outlen,
				cmd->sense);

out_param_list_err:
	return sense_basic_build(cmd->sense, OSD_SSK_ILLEGAL_REQUEST,
				 OSD_ASC_PARAMETER_LIST_LENGTH_ERROR, pid,
				 oid);
}

static int set_attr_value(struct command *cmd, uint64_t pid, uint64_t oid,
			  uint8_t isembedded, uint16_t numoid)
{
	int ret = 0;
	uint64_t i = 0;
	uint8_t *cdb = cmd->cdb;
	uint32_t page = ntohl(&cmd->cdb[64]);
	uint32_t number = ntohl(&cdb[68]);
	uint32_t len = ntohl(&cdb[72]); /* XXX: bug in std? sizeof(len) = 4B */
	uint64_t offset = ntohoffset(&cdb[76]);

	if (page == 0)
		return 0; /* nothing to set. osd2r00 Sec 5.2.2.2 */

	for (i = oid; i < oid+numoid; i++) {
		ret = osd_set_attributes(cmd->osd, pid, i, page, number,
					 &cmd->indata[offset], len, isembedded,
					 cmd->sense);
		if (ret != 0)
			break;
	}

	return ret;
}

/*
 * TODO: proper return codes 
 * returns:
 * ==0: success
 *  >0: failure, senselen is returned.
 */ 
static int get_attr_list(struct command *cmd, uint64_t pid, uint64_t oid,
			 uint8_t isembedded, uint16_t numoid)
{
	int ret = 0;
	uint64_t i = 0;
	uint8_t list_type;
	uint8_t listfmt = RTRVD_SET_ATTR_LIST;
	uint32_t getattr_list_len = ntohl(&cmd->cdb[52]); 
	uint32_t list_len = 0;
	uint64_t list_off = ntohoffset_le(&cmd->cdb[56]);
	uint32_t list_alloc_len = ntohl(&cmd->cdb[60]);
	const uint8_t *list_hdr = &cmd->indata[list_off];
	uint8_t *outbuf = &cmd->outdata[cmd->retrieved_attr_off];
	uint8_t *cp = NULL;

	if (getattr_list_len == 0)
		return 0; /* nothing to retrieve, osd2r00 Sec 5.2.2.3 */

	if (!cmd->outdata)
		goto out_param_list_err;
	outbuf = &cmd->outdata[cmd->retrieved_attr_off];

	if (getattr_list_len != 0 && getattr_list_len < LIST_HDR_LEN)
		goto out_param_list_err;

	if (list_off + list_alloc_len > cmd->outlen)
		goto out_param_list_err;

	if ((list_off & 0x7) || (list_alloc_len & 0x7))
		goto out_param_list_err;

	list_type = list_hdr[0] & 0xF;
	if (list_type != RTRV_ATTR_LIST)
		goto out_invalid_param_list;

	list_len = ntohl(&list_hdr[4]);
	if ((list_len + 8) != getattr_list_len)
		goto out_param_list_err;
	if (list_len & 0x7) /* multiple of 8 */
		goto out_param_list_err;

	if (numoid > 1)
		listfmt = RTRVD_CREATE_MULTIOBJ_LIST;
	outbuf[0] = listfmt; /* fill list header */
	outbuf[1] = outbuf[2] = outbuf[3] = 0;

	list_hdr += 8;
	cp = outbuf + 8;
	cmd->get_used_outlen = 8;
	while (list_len > 0) {
		uint32_t page = ntohl(&list_hdr[0]);
		uint32_t number = ntohl(&list_hdr[4]);

		for (i = oid; i < oid+numoid; i++) {
			uint32_t get_used_outlen;
			ret = osd_getattr_list(cmd->osd, pid, i, page, number,
					       cp, list_alloc_len, isembedded,
					       listfmt, &get_used_outlen,
					       cmd->sense);
			if (ret != 0) {
				cmd->senselen = ret;
				return ret;
			}

			list_alloc_len -= get_used_outlen;
			cmd->get_used_outlen += get_used_outlen;
			cp += get_used_outlen;
		}

		list_len -= 8;
		list_hdr += 8;
	}
	set_htonl_le(&outbuf[4], cmd->get_used_outlen - 8);

	return 0; /* success */

out_param_list_err:
	return sense_basic_build(cmd->sense, OSD_SSK_ILLEGAL_REQUEST,
				 OSD_ASC_PARAMETER_LIST_LENGTH_ERROR, pid,
				 oid);

out_invalid_param_list:
	return sense_basic_build(cmd->sense, OSD_SSK_ILLEGAL_REQUEST,
				 OSD_ASC_INVALID_FIELD_IN_PARAM_LIST, pid,
				 oid);
}

/*
 * TODO: proper return codes 
 * returns:
 * ==0: success
 *  >0: failure, senselen is returned.
 */ 
static int set_attr_list(struct command *cmd, uint64_t pid, uint64_t oid,
			 uint8_t isembedded, uint16_t numoid)
{
	int ret = 0;
	uint64_t i = 0;
	uint8_t list_type;
	uint8_t *cdb = cmd->cdb;
	uint32_t setattr_list_len = ntohl(&cmd->cdb[68]);
	uint32_t list_len = 0;
	uint32_t list_off = ntohoffset(&cmd->cdb[72]);
	const uint8_t *list_hdr = &cmd->indata[list_off];

	if (setattr_list_len == 0)
		return 0; /* nothing to set, osd2r00 Sec 5.2.2.3 */

	if (setattr_list_len != 0 && setattr_list_len < LIST_HDR_LEN)
		goto out_param_list_err;

	list_type = list_hdr[0] & 0xF;
	if (list_type != RTRVD_SET_ATTR_LIST)
		goto out_param_list_err;

	list_len = ntohl(&list_hdr[4]); /* XXX: osd errata */
	if ((list_len + 8) != setattr_list_len)
		goto out_param_list_err;
	if (list_len & 0x7) /* multiple of 8, values are padded */
		goto out_param_list_err;

	list_hdr = &list_hdr[8]; /* XXX: osd errata */
	while (list_len > 0) {
		uint32_t page = ntohl(&list_hdr[0]);
		uint32_t number = ntohl(&list_hdr[4]);
		uint32_t len = ntohs(&list_hdr[8]);
		uint32_t pad = 0;

		/* set attr on multiple objects if that is the case */
		for (i = oid; i < oid+numoid; i++) {
			ret = osd_set_attributes(cmd->osd, pid, i, page, 
						 number, &list_hdr[10], len, 
						 isembedded, cmd->sense);
			if (ret != 0) {
				cmd->senselen = ret;
				return ret;
			}
		}

		pad = (0x8 - ((LE_VAL_OFF + len) & 0x7)) & 0x7;
		list_hdr += LE_VAL_OFF + len + pad;
		list_len -= LE_VAL_OFF + len + pad;
	}

	return 0; /* success */

out_param_list_err:
	cmd->senselen = sense_basic_build(cmd->sense, OSD_SSK_ILLEGAL_REQUEST,
					  OSD_ASC_PARAMETER_LIST_LENGTH_ERROR,
					  pid, oid);
	return cmd->senselen;
}

/*
 * returns:
 * ==0: success
 *  >0: failed, sense len set accordingly
 */
static int get_attributes(struct command *cmd, uint64_t pid, uint64_t oid,
			  uint16_t numoid)
{
	uint8_t isembedded = TRUE;

	if (numoid < 1)
		goto out_cdb_err;

	if (cmd->action == OSD_GET_ATTRIBUTES)
		isembedded = FALSE;

	if (cmd->getset_cdbfmt == GETPAGE_SETVALUE) {
		return get_attr_page(cmd, pid, oid, isembedded, numoid);
	} else if (cmd->getset_cdbfmt == GETLIST_SETLIST) {		
		return get_attr_list(cmd, pid, oid, isembedded, numoid);
	} else {
		goto out_cdb_err;
	}

out_cdb_err:
	return sense_basic_build(cmd->sense, OSD_SSK_ILLEGAL_REQUEST,
				 OSD_ASC_INVALID_FIELD_IN_CDB, pid, oid);

}

/*
 * returns:
 * ==0: success
 *  >0: failed, sense len set accordingly
 */
static int set_attributes(struct command *cmd, uint64_t pid, uint64_t oid,
			  uint32_t numoid)
{
	uint8_t isembedded = TRUE;

	if (numoid < 1)
		goto out_cdb_err;

	if (cmd->action == OSD_SET_ATTRIBUTES)
		isembedded = FALSE;

	if (cmd->getset_cdbfmt == GETPAGE_SETVALUE) {
		return set_attr_value(cmd, pid, oid, isembedded, numoid);
	} else if (cmd->getset_cdbfmt == GETLIST_SETLIST) {		
		return set_attr_list(cmd, pid, oid, isembedded, numoid);
	} else {
		goto out_cdb_err;
	}

out_cdb_err:
	return sense_basic_build(cmd->sense, OSD_SSK_ILLEGAL_REQUEST,
				 OSD_ASC_INVALID_FIELD_IN_CDB, pid, oid);
} 

/*
 * returns:
 * ==0: success
 *  >0: error, cmd->sense is appropriately set
 */
static int cdb_create(struct command *cmd)
{
	int ret = 0;
	uint64_t i = 0;
	uint32_t page;
	uint64_t oid;
	uint8_t *cdb = cmd->cdb;
	uint64_t pid = ntohll(&cdb[16]);
	uint64_t requested_oid = ntohll(&cdb[24]);
	uint16_t numoid = ntohs(&cdb[36]);
	uint8_t local_sense[MAX_SENSE_LEN];

	if (numoid > 1 && cmd->getset_cdbfmt == GETPAGE_SETVALUE) {
		page = ntohl(&cmd->cdb[52]);
		if (page != CUR_CMD_ATTR_PG)
			goto out_cdb_err;
	}

	ret = osd_create(cmd->osd, pid, requested_oid, numoid, cmd->sense);
	if (ret != 0)
		return ret;

	numoid = (numoid == 0) ? 1 : numoid;
	oid = osd_get_created_oid(cmd->osd, numoid);

	ret = set_attributes(cmd, pid, oid, numoid);
	if (ret != 0)
		goto out_remove_obj;
	ret = get_attributes(cmd, pid, oid, numoid);
	if (ret != 0)
		goto out_remove_obj;

	return ret; /* success */

out_remove_obj:
	for (i = oid; i < oid+numoid; i++)
		osd_remove(cmd->osd, pid, i, local_sense); 
	return ret; /* preserve ret */

out_cdb_err:
	ret = sense_basic_build(cmd->sense, OSD_SSK_ILLEGAL_REQUEST,
				OSD_ASC_INVALID_FIELD_IN_CDB, pid,
				requested_oid); 
	return ret;
}

/*
 * returns:
 * ==0: on success
 *  >0: on failure. no side-effects of create partition remain.
 */
static int cdb_create_partition(struct command *cmd)
{
	int ret = 0;
	uint64_t pid = 0;
	uint64_t requested_pid = ntohll(&cmd->cdb[16]);
	uint8_t local_sense[MAX_SENSE_LEN];

	ret = osd_create_partition(cmd->osd, requested_pid, cmd->sense);
	if (ret != 0)
		return ret;

	pid = cmd->osd->ccap.pid;
	ret = set_attributes(cmd, pid, PARTITION_OID, 1);
	if (ret != 0)
		goto out_remove_obj;
	ret = get_attributes(cmd, pid, PARTITION_OID, 1);
	if (ret != 0)
		goto out_remove_obj;

	return ret;

out_remove_obj:
	osd_remove(cmd->osd, pid, PARTITION_OID, local_sense);
	return ret;
}

/*
 * returns:
 * ==0: on success
 *  >0: on failure
 */
static int cdb_remove_partition(struct command *cmd)
{
	int ret = 0;
	uint64_t pid = ntohll(&cmd->cdb[16]);

	ret = set_attributes(cmd, pid, PARTITION_OID, 1);
	if (ret != 0)
		return ret;
	ret = get_attributes(cmd, pid, PARTITION_OID, 1);
	if (ret != 0)
		return ret;

	return osd_remove_partition(cmd->osd, pid, cmd->sense);
}

/*
 * returns:
 * ==0: on success
 *  >0: on failure
 */
static int cdb_remove(struct command *cmd)
{
	int ret = 0;
	uint64_t pid = ntohll(&cmd->cdb[16]);
	uint64_t oid = ntohll(&cmd->cdb[24]);

	ret = set_attributes(cmd, pid, oid, 1);
	if (ret != 0)
		return ret;
	ret = get_attributes(cmd, pid, oid, 1);
	if (ret != 0)
		return ret;

	return osd_remove(cmd->osd, pid, oid, cmd->sense);
}

static inline int std_get_set_attr(struct command *cmd, uint64_t pid, 
				   uint64_t oid)
{
		int ret = set_attributes(cmd, pid, oid, 1);
		if (ret) 
			return ret;

		return get_attributes(cmd, pid, oid, 1);
}

static void exec_service_action(struct command *cmd)
{
	struct osd_device *osd = cmd->osd;
	uint8_t *cdb = cmd->cdb;
	uint8_t *sense = cmd->sense;
	int ret;

	switch (cmd->action) {
	case OSD_APPEND: {
		uint64_t pid = ntohll(&cdb[16]);
		uint64_t oid = ntohll(&cdb[24]);
		uint64_t len = ntohll(&cdb[36]);

		ret = verify_enough_input_data(cmd, len);
		if (ret)
			break;

		ret = osd_append(osd, pid, oid, len, cmd->indata, sense);
		if (ret)
			break;

		ret = std_get_set_attr(cmd, pid, oid);
		break;
	}
	case OSD_CREATE: {
		ret = cdb_create(cmd);
		break;
	}
	case OSD_CREATE_AND_WRITE: {
		uint64_t pid = ntohll(&cdb[16]);
		uint64_t requested_oid = ntohll(&cdb[24]);
		uint64_t len = ntohll(&cdb[36]);
		uint64_t offset = ntohll(&cdb[44]);

		ret = verify_enough_input_data(cmd, len);
		if (ret)
			break;
		ret = osd_create_and_write(osd, pid, requested_oid, len,
					   offset, cmd->indata, sense);
		break;
	}
	case OSD_CREATE_COLLECTION: {
		uint64_t pid = ntohll(&cdb[16]);
		uint64_t requested_cid = ntohll(&cdb[24]);

		ret = osd_create_collection(osd, pid, requested_cid, sense);
		break;
	}
	case OSD_CREATE_PARTITION: {
		ret = cdb_create_partition(cmd);
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

		ret = get_attributes(cmd, pid, oid, 1);
		if (ret)
			break;
		ret = set_attributes(cmd, pid, oid, 1);
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
			       cmd->outdata, &cmd->used_outlen, sense);
		break;
	}
	case OSD_LIST_COLLECTION: {
		uint64_t pid = ntohll(&cdb[16]);
		uint64_t cid = ntohll(&cdb[24]);
		uint32_t list_id = ntohl(&cdb[32]);
		uint64_t alloc_len = ntohll(&cdb[36]);
		uint64_t initial_oid = ntohll(&cdb[44]);
		ret = osd_list_collection(osd, pid, cid, list_id, alloc_len,
					  initial_oid, cmd->outdata,
					  &cmd->used_outlen, sense);
		break;
	}
	case OSD_PERFORM_SCSI_COMMAND:
	case OSD_PERFORM_TASK_MGMT_FUNC:
		ret = osd_error_unimplemented(cmd->action, sense);
		// XXX : uncomment get/set attributes block whenever this gets implemented
		/*if (ret)
			break;

		ret = std_get_set_attr(cmd, pid, oid);*/
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
		ret = osd_read(osd, pid, oid, len, offset, cmd->outdata,
			       &cmd->used_outlen, sense);
		break;
	}
	case OSD_REMOVE: {
		ret = cdb_remove(cmd);
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
		ret = cdb_remove_partition(cmd);
		break;
	}
	case OSD_SET_ATTRIBUTES: {
		uint64_t pid = ntohll(&cdb[16]);
		uint64_t oid = ntohll(&cdb[24]);

		ret = set_attributes(cmd, pid, oid, 1);
		if (ret)
			break;
		ret = get_attributes(cmd, pid, oid, 1);
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
					 alloc_len, cmd->outdata,
					 &cmd->used_outlen, sense);
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
		ret = verify_enough_input_data(cmd, len);
		if (ret)
			break;
		ret = osd_write(osd, pid, oid, len, offset, cmd->indata,
				sense);
		break;
	}
	default:
		ret = osd_error_unimplemented(cmd->action, sense);
	}

	/*
	 * All the above return bytes of sense data put into sense[]
	 * in case of an error.  But sometimes they also return good
	 * data.  Just save the senselen here and continue.
	 */
	cmd->senselen = ret;
}


/*
 * uaddr: either input or output, depending on the cmd
 * uaddrlen: output var with number of valid bytes put in uaddr after a read
 */
int osdemu_cmd_submit(struct osd_device *osd, uint8_t *cdb,
		      const uint8_t *data_in, uint64_t data_in_len,
		      uint8_t **data_out, uint64_t *data_out_len,
		      uint8_t *sense_out, int *senselen_out)
{
	int ret = 0;
	struct command cmd = {
		.osd = osd,
		.cdb = cdb,
		.action = (cdb[8] << 8) | cdb[9],
		.getset_cdbfmt = (cdb[11] & 0x30) >> 4,
		.indata = data_in,
		.inlen = data_in_len,
		.outdata = NULL,
		.used_outlen = 0,
		.get_used_outlen = 0,
		.sense = { 0 },  /* maybe no need to initialize to zero */
		.senselen = 0,
	};

	/* check cdb opcode and length */
	if (cdb[0] != CDB_OPCODE || cdb[7] != ADD_CDB_LEN)
		goto out_opcode_err;

	/* Sets retrieved_attr_off, outlen. */
	ret = calc_max_out_len(&cmd);
	if (ret < 0)
		goto out_cdb_err;

	/* Allocate total possible output size. */
	if (cmd.outlen) {
		cmd.outdata = Malloc(cmd.outlen); /* XXX: stgt will free it */
		if (!cmd.outdata)
			goto out_hw_err;
	}

	exec_service_action(&cmd); /* run the command. */

	/* any error sets senselen to >0 */
	if (cmd.senselen > 0)
		goto out_free_resource;

	if (cmd.get_used_outlen > 0) {
		if (cmd.used_outlen < cmd.retrieved_attr_off) {
			/*
			 * Not supposed to touch these bytes, but no way to
			 * have discontiguous return blocks.  So fill with 0.
			 */
			memset(cmd.outdata + cmd.used_outlen, 0,
			       cmd.retrieved_attr_off - cmd.used_outlen);
		}
		cmd.used_outlen = cmd.retrieved_attr_off
			+ cmd.get_used_outlen;
	}

	/* Return the data buffer and get attributes. */
	if (cmd.outlen > 0) {
		if (cmd.used_outlen > 0) {
			*data_out = cmd.outdata;
			*data_out_len = cmd.used_outlen;
		} else {
			goto out_free_resource;
		}
	}
	goto out;

out_opcode_err:
	cmd.senselen = sense_header_build(cmd.sense, MAX_SENSE_LEN,
					  OSD_SSK_ILLEGAL_REQUEST,
					  OSD_ASC_INVALID_COMMAND_OPCODE, 0);
	goto out_free_resource;

out_cdb_err:
	cmd.senselen = sense_header_build(cmd.sense, MAX_SENSE_LEN,
					  OSD_SSK_ILLEGAL_REQUEST,
					  OSD_ASC_INVALID_FIELD_IN_CDB, 0); 
	goto out_free_resource;

out_hw_err:
	cmd.senselen = sense_header_build(cmd.sense, MAX_SENSE_LEN,
					  OSD_SSK_HARDWARE_ERROR,
					  OSD_ASC_SYSTEM_RESOURCE_FAILURE, 0); 
	goto out_free_resource;

out_free_resource:
	free(cmd.outdata);

out:
	if (cmd.senselen == 0) {
		return SAM_STAT_GOOD;
	} else {
		/* valid sense data, length is ret, maybe good data too */
		*senselen_out = cmd.senselen;
		memcpy(sense_out, cmd.sense, cmd.senselen);
		return SAM_STAT_CHECK_CONDITION;
	}
}

/*
 * Return 0 if all okay.  Return >0 if some sense data was created,
 * this is the length of the sense data.  The input sense buffer is
 * known to be sized large enough to hold anything (MAX_SENSE_LEN).
 * Output data goes into data_out, and puts the length in data_out_len, up to
 * the data_out_len_max.
 *
 * The data_out buf is already allocated and pointing at the retrieved
 * attributes offset.  Just fill it up to _max and report how much you
 * used.
 */
