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
static int verify_enough_input_data(struct command *command, uint64_t cdblen)
{
	int ret = 0;

	if (cdblen != command->inlen) {
		osd_error("%s: supplied data %llu but cdb says %llu\n",
		      __func__, llu(command->inlen), llu(cdblen));
		ret = osd_error_bad_cdb(command->sense);
	}
	return ret;
}

static uint16_t get_list_item(uint8_t *list, int num)
{
	return ntohl(&list[3 + num*2]);
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
static void get_attributes(struct command *command)
{	
	int ret;
	uint64_t pid = ntohll(&command->cdb[16]);
	uint64_t oid = ntohll(&command->cdb[24]);
	uint8_t *outdata = command->outdata + command->retrieved_attr_off;
	uint64_t outlen = command->outlen - command->retrieved_attr_off;

	/* if =2, get an attribute page and set an attribute value */
	if (command->getset_cdbfmt == 2)
	{
		uint32_t page = ntohl(&command->cdb[52]);
		
		/* if page = 0, no attributes are to be gotten */
		if (page != 0)
		{
			uint16_t len = outlen;

			ret = osd_get_attributes(command->osd, pid, oid,
				page, 0, outdata, &len, 1, EMBEDDED,
				command->sense);
			if (ret > 0)
				command->senselen = ret;
			command->get_used_outlen = len;
		}
	}

	/* else if =3, get attributes using lists */
	if (command->getset_cdbfmt == 3)
	{
		uint32_t get_list_len = ntohl(&command->cdb[52]);

		if (get_list_len != 0)
		{
			uint32_t get_list_offset = ntohl(&command->cdb[56]);

			uint32_t tmp = 0;
			uint16_t list_item;
	
			for( tmp=0; tmp<get_list_len; tmp++)
			{
				list_item = get_list_item(outdata, tmp);
				/*XXX: get list item attr */
				/*XXX: Check for errors/sense */
			}
		}
						
	}
}

static int set_attributes(struct osd_device *osd, uint8_t *cdb,
                          uint8_t *data_in, uint64_t data_in_len,
		          uint8_t *sense, uint8_t getset_cdbfmt)
{
	int ret = 0;
	uint64_t pid = ntohll(&cdb[16]);
	uint64_t oid = ntohll(&cdb[24]);
	
	/* if =2, set an attribute value */
	if (getset_cdbfmt == 2)
	{
		uint32_t set_page = ntohl(&cdb[64]);
		
		if (set_page != 0){
			uint32_t set_num = ntohl(&cdb[68]);
			uint32_t set_len = ntohl(&cdb[72]);
			uint32_t set_offset = ntohl(&cdb[76]);
			
			ret = osd_set_attributes(osd, pid, oid,
                      	 	set_page, set_num, &data_in[set_offset],
		       		set_len, EMBEDDED, sense);
			/*XXX: Check for errors/sense */
		}
	}
	/* else if =3, set attributes using lists */
	if (getset_cdbfmt == 3)
	{		
		uint32_t set_list_len = ntohl(&cdb[68]);
		
		if (set_list_len != 0)
		{

			uint32_t set_list_offset = ntohl(&cdb[72]);
			
			uint32_t tmp;
			uint16_t list_item;
	
			for( tmp=0; tmp<set_list_len; tmp++)
			{
				list_item = get_list_item(data_in, tmp);
				/*XXX - set list item attr */
				/*XXX: Check for errors/sense */
			}
		}

	}
	return ret;
} 

/*
 * What's the total number of bytes this command might produce?
 */
static void calc_max_out_len(struct command *command)
{
	uint64_t end;

	/*
	 * These commands return data.
	 */
	switch (command->action) {
	case OSD_LIST:
	case OSD_LIST_COLLECTION:
	case OSD_READ:
	case OSD_SET_MASTER_KEY:
		command->outlen = ntohll(&command->cdb[36]);
		break;
	default:
		command->outlen = 0;
	}

	/*
	 * All commands might have getattr requests.  Add together
	 * the result offset and the allocation length to get the
	 * end of their write.
	 */
	command->retrieved_attr_off = 0;
	end = 0;
	if (command->getset_cdbfmt == 2) {
		command->retrieved_attr_off = ntohoffset(&command->cdb[60]);
		end = command->retrieved_attr_off + ntohl(&command->cdb[56]);
	}
	if (command->getset_cdbfmt == 3) {
		command->retrieved_attr_off = ntohoffset(&command->cdb[64]);
		end = command->retrieved_attr_off + ntohl(&command->cdb[60]);
	}

	if (end > command->outlen)
		command->outlen = end;
}

/*
 * Run the relevant command.
 */
static void do_action(struct command *command)
{
	struct osd_device *osd = command->osd;
	uint8_t *cdb = command->cdb;
	uint8_t *sense = command->sense;
	int ret;

	switch (command->action) {
	case OSD_APPEND: {
		uint64_t pid = ntohll(&cdb[16]);
		uint64_t oid = ntohll(&cdb[24]);
		uint64_t len = ntohll(&cdb[36]);
		ret = verify_enough_input_data(command, len);
		if (ret)
			break;
		ret = osd_append(osd, pid, oid, len, command->indata, sense);
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
		ret = verify_enough_input_data(command, len);
		if (ret)
			break;
		ret = osd_create_and_write(osd, pid, requested_oid, len,
		                           offset, command->indata, sense);
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

		// malloc(dataout) first based on passed-in expected length
		// from user, pass this to the functions
		//
		// dataout = malloc(expected_from_initiator);
		// ret = get_attributes(..., dataout, data_out_len, sense);
		// Check for errors, return debug/sense
		//
		// ret = set_attributes();
		// Check for errors, return debug/sense
		//
		// return this new array to initiator
		// *data_out = dataout;
		ret = 0;
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
			       command->outdata, &command->used_outlen, sense);
		break;
	}
	case OSD_LIST_COLLECTION: {
		uint64_t pid = ntohll(&cdb[16]);
		uint64_t cid = ntohll(&cdb[24]);
		uint32_t list_id = ntohl(&cdb[32]);
		uint64_t alloc_len = ntohll(&cdb[36]);
		uint64_t initial_oid = ntohll(&cdb[44]);
		ret = osd_list_collection(osd, pid, cid, list_id, alloc_len,
		                          initial_oid, command->outdata,
					  &command->used_outlen, sense);
		break;
	}
	case OSD_PERFORM_SCSI_COMMAND:
	case OSD_PERFORM_TASK_MGMT_FUNC:
		ret = osd_error_unimplemented(command->action, sense);
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
		ret = osd_read(osd, pid, oid, len, offset, command->outdata,
		               &command->used_outlen, sense);
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
		// ret = set_attributes();
		// Check for errors, return debug/sense
		// ret = get_attributes();
		// Check for errors, return debug/sense
		ret = 0;
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
		                         alloc_len, command->outdata,
					 &command->used_outlen, sense);
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
		ret = verify_enough_input_data(command, len);
		if (ret)
			break;
		ret = osd_write(osd, pid, oid, len, offset, command->indata,
		                sense);
		break;
	}
	default:
		ret = osd_error_unimplemented(command->action, sense);
	}

	/*
	 * All the above return bytes of sense data put into sense[]
	 * in case of an error.  But sometimes they also return good
	 * data.  Just save the senselen here and continue.
	 */
	command->senselen = ret;
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
	int ret;
	struct command command = {
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

	if (command.getset_cdbfmt != 2 && command.getset_cdbfmt != 3) {
		command.senselen = sense_header_build(command.sense,
			MAX_SENSE_LEN, OSD_SSK_ILLEGAL_REQUEST,
			OSD_ASC_INVALID_FIELD_IN_CDB, 0);
		goto outsense;
	}

	/*
	 * Sets retrieved_attr_off, outlen.
	 */
	calc_max_out_len(&command);

	/*
	 * Allocate total possible output size.
	 */
	if (command.outlen) {
		command.outdata = malloc(command.outlen);
		if (!command.outdata) {
			osd_error("%s: malloc %llu failed", __func__,
			      llu(command.outlen));
			command.senselen = sense_header_build(command.sense,
				MAX_SENSE_LEN, OSD_SSK_HARDWARE_ERROR,
				OSD_ASC_SYSTEM_RESOURCE_FAILURE, 0);
			goto outsense;
		}
	}

	/*
	 * Run the command.
	 */
	do_action(&command);


	/*
	 * Process optional get/set attr arguments.
	 */
	/* set_attributes */
		    
	get_attributes(&command);

	if (command.get_used_outlen > 0) {
		if (command.used_outlen < command.retrieved_attr_off) {
		    /*
		     * Not supposed to touch these bytes, but no way to
		     * have discontiguous return blocks.  So fill with 0.
		     */
		    memset(command.outdata + command.used_outlen, 0,
			   command.retrieved_attr_off - command.used_outlen);
		}
		command.used_outlen = command.retrieved_attr_off
		                    + command.get_used_outlen;
	}

	/* Return the data buffer and get attributes. */
	if (command.outlen > 0) {
		if (command.used_outlen > 0) {
			*data_out = command.outdata;
			*data_out_len = command.used_outlen;
		} else {
			/* Some sort of problem that wrote no bytes. */
			free(command.outdata);
		}
	}

	if (ret < 0) {
		osd_error("%s: ret should never be negative here", __func__);
		ret = 0;
	} else if (ret == 0) {
		ret = SAM_STAT_GOOD;
	} else {
outsense:
		/* valid sense data, length is ret, maybe good data too */
		*sense_out = malloc(command.senselen);
		if (*sense_out) {
			*senselen_out = command.senselen;
			memcpy(*sense_out, command.sense, command.senselen);
		}
		ret = SAM_STAT_CHECK_CONDITION;
	}
	/* return a SAM_STAT upwards */
	return ret;
}



