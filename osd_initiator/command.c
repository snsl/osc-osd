#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <scsi/scsi.h>

#include "util/util.h"
#include "command.h"

static void varlen_cdb_init(struct osd_command *command, uint16_t action)
{
	memset(command, 0, sizeof(*command));
	command->cdb_len = OSD_CDB_SIZE;
	command->cdb[0] = VARLEN_CDB;
	/* we do not support ACA or LINK in control byte cdb[1], leave as 0 */
	command->cdb[7] = OSD_CDB_SIZE - 8;
        command->cdb[8] = (action & 0xff00U) >> 8;
        command->cdb[9] = (action & 0x00ffU);
	command->cdb[11] = 3 << 4;  /* default to list, but empty list lens */
	/* Update timestamps based on action 5.2.8 */
	command->cdb[12] = 0x7f;  /* timestamps not updated */
}

/*
 * Non-varlen commands.
 */
int osd_command_set_test_unit_ready(struct osd_command *command)
{
	memset(command, 0, sizeof(*command));
	command->cdb_len = 6;
	return 0;
}

int osd_command_set_inquiry(struct osd_command *command, uint8_t outlen)
{
	memset(command, 0, sizeof(*command));
	command->cdb_len = 6;
	command->cdb[0] = INQUIRY;
	command->cdb[4] = outlen;
	return 0;
}

/*
 * These functions take a cdb of the appropriate size (200 bytes),
 * and the arguments needed for the particular command.  They marshall
 * the arguments but do not submit the command.
 */     
int osd_command_set_append(struct osd_command *command, uint64_t pid,
			   uint64_t oid, uint64_t len)
{
        varlen_cdb_init(command, OSD_APPEND);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], oid);
        set_htonll(&command->cdb[36], len);
        return 0;
}


int osd_command_set_create(struct osd_command *command, uint64_t pid,
			   uint64_t requested_oid, uint16_t num_user_objects)
{
        varlen_cdb_init(command, OSD_CREATE);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], requested_oid);
        set_htons(&command->cdb[36], num_user_objects);
        return 0;
}


int osd_command_set_create_and_write(struct osd_command *command, uint64_t pid,
				     uint64_t requested_oid, uint64_t len,
				     uint64_t offset)
{
        varlen_cdb_init(command, OSD_CREATE_AND_WRITE);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], requested_oid);
        set_htonll(&command->cdb[36], len);
        set_htonll(&command->cdb[44], offset);
        return 0;
}


int osd_command_set_create_collection(struct osd_command *command,
				      uint64_t pid, uint64_t requested_cid)
{
        varlen_cdb_init(command, OSD_CREATE_COLLECTION);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], requested_cid);
        return 0;
}

int osd_command_set_create_partition(struct osd_command *command,
				     uint64_t requested_pid)
{
        varlen_cdb_init(command, OSD_CREATE_PARTITION);
        set_htonll(&command->cdb[16], requested_pid);
        return 0;
}


int osd_command_set_flush(struct osd_command *command, uint64_t pid,
			  uint64_t oid, int flush_scope)
{
        varlen_cdb_init(command, OSD_FLUSH);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], oid);
        command->cdb[10] = (command->cdb[10] & ~0x3) | flush_scope;
        return 0;
}


int osd_command_set_flush_collection(struct osd_command *command, uint64_t pid,
				     uint64_t cid, int flush_scope)
{
        varlen_cdb_init(command, OSD_FLUSH_COLLECTION);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], cid);
        command->cdb[10] = (command->cdb[10] & ~0x3) | flush_scope;
        return 0;
}


int osd_command_set_flush_osd(struct osd_command *command, int flush_scope)
{
        varlen_cdb_init(command, OSD_FLUSH_OSD);
        command->cdb[10] = (command->cdb[10] & ~0x3) | flush_scope;
        return 0;
}


int osd_command_set_flush_partition(struct osd_command *command, uint64_t pid,
				    int flush_scope)
{
        varlen_cdb_init(command, OSD_FLUSH_PARTITION);
        set_htonll(&command->cdb[16], pid);
        command->cdb[10] = (command->cdb[10] & ~0x3) | flush_scope;
        return 0;
}

/*
 * Destroy the db and start over again.
 */
int osd_command_set_format_osd(struct osd_command *command, uint64_t capacity)
{
        varlen_cdb_init(command, OSD_FORMAT_OSD);
        set_htonll(&command->cdb[36], capacity);
        return 0;
}


int osd_command_set_get_attributes(struct osd_command *command, uint64_t pid,
				   uint64_t oid)
{
        varlen_cdb_init(command, OSD_GET_ATTRIBUTES);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], oid);
        return 0;
}


int osd_command_set_get_member_attributes(struct osd_command *command,
					  uint64_t pid, uint64_t cid)
{
        varlen_cdb_init(command, OSD_GET_MEMBER_ATTRIBUTES);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], cid);
        return 0;
}


int osd_command_set_list(struct osd_command *command, uint64_t pid,
			 uint32_t list_id, uint64_t alloc_len,
			 uint64_t initial_oid)
{
        varlen_cdb_init(command, OSD_LIST);
        set_htonll(&command->cdb[16], pid);
        set_htonl(&command->cdb[32], list_id);
        set_htonll(&command->cdb[36], alloc_len);
        set_htonll(&command->cdb[44], initial_oid);
        return 0;
}

int osd_command_set_list_collection(struct osd_command *command, uint64_t pid,
				    uint64_t cid, uint32_t list_id,
				    uint64_t alloc_len, uint64_t initial_oid)
{
        varlen_cdb_init(command, OSD_LIST_COLLECTION);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], cid);
        set_htonl(&command->cdb[32], list_id);
        set_htonll(&command->cdb[36], alloc_len);
        set_htonll(&command->cdb[44], initial_oid);
        return 0;
}

int osd_command_set_perform_scsi_command(struct osd_command *command __unused)
{
        osd_error("%s: unimplemented", __func__);
        return 1;
}

int osd_command_set_perform_task_mgmt_func(struct osd_command *command __unused)
{
        osd_error("%s: unimplemented", __func__);
        return 1;
}

int osd_command_set_query(struct osd_command *command, uint64_t pid,
			  uint64_t cid, uint32_t query_len, uint64_t alloc_len)
{
        varlen_cdb_init(command, OSD_QUERY);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], cid);
        set_htonl(&command->cdb[32], query_len);
        set_htonll(&command->cdb[36], alloc_len);
        return 0;
}

int osd_command_set_read(struct osd_command *command, uint64_t pid,
			 uint64_t oid, uint64_t len, uint64_t offset)
{
        varlen_cdb_init(command, OSD_READ);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], oid);
        set_htonll(&command->cdb[36], len);
        set_htonll(&command->cdb[44], offset);
        return 0;
}


int osd_command_set_remove(struct osd_command *command, uint64_t pid,
			   uint64_t oid)
{
        varlen_cdb_init(command, OSD_REMOVE);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], oid);
        return 0;
}


int osd_command_set_remove_collection(struct osd_command *command,
				      uint64_t pid, uint64_t cid)
{
        varlen_cdb_init(command, OSD_REMOVE_COLLECTION);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], cid);
        return 0;
}


int osd_command_set_remove_member_objects(struct osd_command *command,
					  uint64_t pid, uint64_t cid)
{
        varlen_cdb_init(command, OSD_REMOVE_MEMBER_OBJECTS);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], cid);
        return 0;
}


int osd_command_set_remove_partition(struct osd_command *command, uint64_t pid)
{
        varlen_cdb_init(command, OSD_REMOVE_PARTITION);
        set_htonll(&command->cdb[16], pid);
        return 0;
}


int osd_command_set_set_attributes(struct osd_command *command, uint64_t pid,
				   uint64_t oid)
{
        varlen_cdb_init(command, OSD_SET_ATTRIBUTES);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], oid);
        return 0;
}


int osd_command_set_set_key(struct osd_command *command, int key_to_set,
			    uint64_t pid, uint64_t key, const uint8_t seed[20])
{
        varlen_cdb_init(command, OSD_SET_KEY);
        command->cdb[11] = (command->cdb[11] & ~0x3) | key_to_set;
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], key);
        memcpy(&command->cdb[32], seed, 20);
        return 0;
}


int osd_command_set_set_master_key(struct osd_command *command, int dh_step,
				   uint64_t key, uint32_t param_len,
				   uint32_t alloc_len)
{
        varlen_cdb_init(command, OSD_SET_KEY);
        command->cdb[11] = (command->cdb[11] & ~0x3) | dh_step;
        set_htonll(&command->cdb[24], key);
        set_htonl(&command->cdb[32], param_len);
        set_htonl(&command->cdb[36], alloc_len);
        return 0;
}


int osd_command_set_set_member_attributes(struct osd_command *command,
					  uint64_t pid, uint64_t cid)
{
        varlen_cdb_init(command, OSD_SET_MEMBER_ATTRIBUTES);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], cid);
        return 0;
}


int osd_command_set_write(struct osd_command *command, uint64_t pid,
			  uint64_t oid, uint64_t len, uint64_t offset)
{
        varlen_cdb_init(command, OSD_WRITE);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], oid);
        set_htonll(&command->cdb[36], len);
        set_htonll(&command->cdb[44], offset);
        return 0;
}

/*
 * Header for internal use across attr_build to attr_resolve.  Keeps
 * the original iov structures.
 */
struct attr_malloc_header {
	const void *orig_outdata;
	size_t orig_outlen;
	int orig_iov_outlen;
	void *orig_indata;
	size_t orig_inlen_alloc;
	size_t orig_inlen;
	int orig_iov_inlen;
	uint8_t *retrieved_buf;
	uint64_t start_retrieved;
	/* for convenience of resolve, points into the big buffer */
	struct attribute_get_multi_results *mr;
};

/*
 * After setting the command (i.e. building the CDB), and connecting
 * the indata and outdata, this function tacks on a list of attributes.
 * To do this it will possibly create a new buffer for output data
 * containing the request, and it will possibly create a new buffer for
 * input data with the GET responses.  And it will build an iovec to
 * hold the command data as well as the attribute data.
 *
 * After the command completes, call the resolve function to fill in
 * the results and free the temp buffers.
 */
int osd_command_attr_build(struct osd_command *command,
                           const struct attribute_list *attr, int numattr)
{
	int use_getpage;
	int numget, numgetpage, numgetmulti, numset;
	uint32_t getsize, getpagesize, getmultisize, setsize;
	uint32_t getmulti_num_objects;
	struct attr_malloc_header *header;
	uint8_t *p;
	struct bsg_iovec *iov;
	int i;
	int neediov;
	int setattr_index;
	uint64_t extra_out, extra_in;
	uint32_t getmulti_result_space;

	/*
	 * These are values, in units of bytes (not offsets), in the
	 * out data.
	 */
	uint64_t end_outdata;
	uint64_t size_pad_outdata;
	uint64_t start_getlist;
	uint64_t size_getlist;
	uint64_t size_pad_getlist;
	uint64_t start_setlist;
	uint64_t size_setlist;

	/*
	 * Ditto, in data.
	 */
	uint64_t end_indata;
	uint64_t size_pad_indata;
	uint64_t start_retrieved;
	uint64_t size_retrieved;

	if (numattr == 0)
		return 0;

	/*
	 * Figure out what operations to do.  That will lead to constraints
	 * on the choice of getpage/setone versus getlist/setlist format.
	 */
	numget = numgetpage = numgetmulti = numset = 0;
	getsize = getpagesize = getmultisize = setsize = 0;
	setattr_index = -1;
	for (i=0; i<numattr; i++) {
		if (attr[i].type == ATTR_GET) {
			++numget;
			getsize += roundup8(10 + attr[i].len);
		} else if (attr[i].type == ATTR_GET_PAGE) {
			++numgetpage;
			getpagesize += attr[i].len;  /* no round */
		} else if (attr[i].type == ATTR_GET_MULTI) {
			++numgetmulti;
			getmultisize += roundup8(18 + attr[i].len);
		} else if (attr[i].type == ATTR_SET) {
			++numset;
			setattr_index = i;
			setsize += roundup8(10 + attr[i].len);
		} else {
			osd_error("%s: invalid attribute type %d", __func__,
			          attr[i].type);
			return -EINVAL;
		}
	}

	if (numgetpage) {
		if (numgetpage > 1) {
			osd_error("%s: only one ATTR_GET_PAGE at a time",
				  __func__);
			return -EINVAL;
		}
		if (numget) {
			osd_error("%s: no ATTR_GET allowed with ATTR_GET_PAGE",
				  __func__);
			return -EINVAL;
		}
		if (numgetmulti) {
			osd_error(
			    "%s: no ATTR_GETMULTI allowed with ATTR_GET_PAGE",
				  __func__);
			return -EINVAL;
		}
		if (numset > 1) {
			osd_error("%s: ATTR_GET_PAGE limits ATTR_SET to one",
				  __func__);
			return -EINVAL;
		}
		use_getpage = 1;
	} else {
		if (numgetmulti) {
			uint16_t action = (command->cdb[8] << 8)
			                 | command->cdb[9];
			if (numget) {
				osd_error(
				"%s: no ATTR_GET allowed with ATTR_GET_MULTI",
					  __func__);
				return -EINVAL;
			}
			if (action != OSD_CREATE) {
				osd_error(
			    "%s: ATTR_GET_MULTI must only be used with CREATE",
					  __func__);
				return -EINVAL;
			}
			getmulti_num_objects = ntohs(&command->cdb[36]);
		}
		use_getpage = 0;
		if (numget == 0 && numgetmulti == 0 && numset == 1)
			/*
			 * For a single set, prefer simpler page format,
			 * except if page-to-set is zero, which is a noop
			 * in this format.
			 */
			if (attr[setattr_index].page != 0)
				use_getpage = 1;
	}

	/*
	 * Outdata:
	 *	padding between real outdata and getlist
	 *	getlist
	 *	padding between getlist and setlist
	 *	setlist (including values)
	 */
	end_outdata = command->outlen;
	start_getlist = next_offset(end_outdata);
	size_pad_outdata = start_getlist - end_outdata;

	size_getlist = 0;
	if (numget)
		size_getlist = 8 + 8 * numget;
	if (numgetmulti)
		size_getlist = 8 + 8 * numgetmulti;

	start_setlist = 0;
	size_pad_getlist = 0;
	size_setlist = 0;
	if (numset) {
		start_setlist = next_offset(start_getlist + size_getlist);
		size_pad_getlist = start_setlist
				 - (start_getlist + size_getlist);
		if (use_getpage) {
			/* no list format around the value */
			size_setlist = attr[setattr_index].len;
		} else {
			size_setlist = 8 + setsize;  /* already rounded */
		}
	}
	extra_out = size_pad_outdata + size_getlist
		  + size_pad_getlist + size_setlist;

	/*
	 * Indata:
	 *	padding between real indata and retrieved results area
	 *	retrieved results area
	 */
	end_indata = command->inlen;
	start_retrieved = next_offset(end_indata);
	size_pad_indata = start_retrieved - end_indata;

	size_retrieved = 0;
	if (numget)
		size_retrieved = 8 + getsize;  /* already rounded */
	if (numgetpage)
		size_retrieved = getpagesize;  /* no rounding */
	if (numgetmulti)
		size_retrieved = 8 + getmultisize * getmulti_num_objects;
	extra_in = size_pad_indata + size_retrieved;

	/*
	 * Each ATTR_GET_MULTI will return an array of values, one for
	 * each object.  Allocate space for that now, for use by
	 * attr_resolve();
	 */
	getmulti_result_space = 0;
	if (numgetmulti) {
		struct attribute_get_multi_results *mr;
		int one_space = sizeof(*mr)
			+ getmulti_num_objects
			* (sizeof(*mr->oid) + sizeof(*mr->val)
			   + sizeof(*mr->outlen));
		getmulti_result_space = numgetmulti * one_space;
	}

	/*
	 * If we are building out or indata items, and user already
	 * had some data there, build replacement iovecs to hold the
	 * new list.
	 */
	neediov = 0;
	if (extra_out && command->outdata) {
		if (command->iov_outlen == 0)
			neediov += 2;
		else
			neediov += command->iov_outlen + 1;
	}
	if (extra_in && command->indata) {
		if (command->iov_inlen == 0)
			neediov += 2;
		else
			neediov += command->iov_inlen + 1;
	}

	/*
	 * Allocate space for the following things, in order:
	 * 	header struct of original iov/data, etc.
	 * 	attr array
	 * 	get_multi results
	 * 	out iovec
	 * 	in iovec
	 * 	outgoing getlist area
	 * 	outgoing setlist area
	 * 	incoming retrieved area
	 */
	command->attr_malloc = Malloc(sizeof(*header)
				    + numattr * sizeof(*attr)
				    + getmulti_result_space
				    + neediov * sizeof(*iov)
				    + extra_out + extra_in);
	if (!command->attr_malloc)
		return 1;

	/* header holds original indata, iniovec, etc. */
	header = command->attr_malloc;

	/* copy user's passed-in attribute structure for use by resolve */
	command->attr = (void *) &header[1];
	memcpy(command->attr, attr, numattr * sizeof(*attr));
	command->numattr = numattr;

	/* for ATTR_GET_MULTI results */
	header->mr = (void *) &command->attr[numattr];

	/* space for replacement out and in iovecs */
	iov = (void *) ((uint8_t *) header->mr + getmulti_result_space);

	/* outgoing pad, getlist, pad, setlist; incoming pad, retrieval areas */
	p = (uint8_t *) &iov[neediov];

	/*
	 * Initialize get multi result space.
	 */
	if (getmulti_result_space) {
		struct attribute_get_multi_results *mr = header->mr;
		uint8_t *q = (void *) &mr[numgetmulti];
		for (i=0; i<numgetmulti; i++) {
			mr[i].numoid = 0;
			mr[i].oid = (void *) q;
			q += getmulti_num_objects * sizeof(*mr[i].oid);
			mr[i].val = (void *) q;
			q += getmulti_num_objects * sizeof(*mr[i].val);
			mr[i].outlen = (void *) q;
			q += getmulti_num_objects * sizeof(*mr[i].outlen);
		}
	}

	/* set pad spaces to known pattern, for debugging */
	memset(p, 0x6b, size_pad_outdata);
	p += size_pad_outdata;

	/*
	 * Build the getlist.
	 */
	if (numget || numgetmulti) {
		uint8_t *q = p + 8;
		for (i=0; i<numattr; i++) {
			if (!(attr[i].type == ATTR_GET_MULTI
			   || attr[i].type == ATTR_GET))
				continue;
			set_htonl(&q[0], attr[i].page);
			set_htonl(&q[4], attr[i].number);
			q += 8;
		}
		p[0] = 0x1;
		p[1] = p[2] = p[3] = 0;
		set_htonl(&p[4], (numget + numgetmulti) * 8);
		p += size_getlist;
	}

	memset(p, 0x6b, size_pad_getlist);
	p += size_pad_getlist;

	/*
	 * Build the setlist, or copy in the single value to set.
	 */
	if (numset) {
		if (use_getpage) {
			memcpy(p, attr[setattr_index].val,
			       attr[setattr_index].len);
		} else {
			uint8_t *q = p + 8;
			for (i=0; i<numattr; i++) {
				int len;
				if (attr[i].type != ATTR_SET)
					continue;
				set_htonl(&q[0], attr[i].page);
				set_htonl(&q[4], attr[i].number);
				set_htons(&q[8], attr[i].len);
				memcpy(&q[10], attr[i].val, attr[i].len);
				len = 10 + attr[i].len;
				q += len;
				while (len & 7) {
					*q++ = 0;
					++len;
				}
			}
			p[0] = 0x9;
			p[1] = p[2] = p[3] = 0;
			set_htonl(&p[4], setsize);
		}
		p += size_setlist;
	}
	/* no padding at the end of the setlist */

	/* Now we are at the beginning of the retrieved area */
	memset(p, 0x6b, size_pad_indata);
	p += size_pad_indata;

	/* Set it to something different, for debugging */
	header->retrieved_buf = p;
	header->start_retrieved = start_retrieved;
	memset(p, 0x7c, size_retrieved);

	/*
	 * Update the CDB bits.
	 */
	if (use_getpage) {
		/* page format */
		command->cdb[11] = (command->cdb[11] & ~(3 << 4)) | (2 << 4);
		for (i=0; i<numattr; i++) {
			if (attr[i].type == ATTR_GET_PAGE)
				set_htonl(&command->cdb[52], attr[i].page);
			if (attr[i].type == ATTR_SET) {
				set_htonl(&command->cdb[64], attr[i].page);
				set_htonl(&command->cdb[68], attr[i].number);
			}
		}
		set_htonl(&command->cdb[56], size_retrieved);
		set_htonoffset(&command->cdb[60], start_retrieved);
		set_htonl(&command->cdb[72], size_setlist);
		set_htonoffset(&command->cdb[76], start_setlist);
	} else {
		/* list format, cdb[11] is this as default */
		set_htonl(&command->cdb[52], size_getlist);
		set_htonoffset(&command->cdb[56], start_getlist);
		set_htonl(&command->cdb[60], size_retrieved);
		set_htonoffset(&command->cdb[64], start_retrieved);
		set_htonl(&command->cdb[68], size_setlist);
		set_htonoffset(&command->cdb[72], start_setlist);
	}

	/*
	 * Adjust the iovecs.
	 */
	p = (uint8_t *) &iov[neediov];
	header->orig_outdata = command->outdata;
	header->orig_outlen = command->outlen;
	header->orig_iov_outlen = command->iov_outlen;
	if (extra_out) {
		if (command->outdata) {
			if (command->iov_outlen == 0) {
				iov->iov_base = (uintptr_t) command->outdata;
				iov->iov_len = command->outlen;
				++iov;
				command->iov_outlen = 2;
			} else {
				memcpy(iov, command->outdata,
				       command->iov_outlen * sizeof(*iov));
				iov += command->iov_outlen;
				++command->iov_outlen;
			}
			iov->iov_base = (uintptr_t) p;
			iov->iov_len = extra_out;
			++iov;
		} else {
			command->outdata = p;
		}
		command->outlen += extra_out;
	}

	p += extra_out;
	header->orig_indata = command->indata;
	header->orig_inlen_alloc = command->inlen_alloc;
	header->orig_inlen = command->inlen;
	header->orig_iov_inlen = command->iov_inlen;
	if (extra_in) {
		if (command->indata) {
			if (command->iov_inlen == 0) {
				iov->iov_base = (uintptr_t) command->indata;
				iov->iov_len = command->inlen;
				++iov;
				command->iov_inlen = 2;
			} else {
				memcpy(iov, command->indata,
				       command->iov_inlen * sizeof(*iov));
				iov += command->iov_inlen;
				++command->iov_inlen;
			}
			iov->iov_base = (uintptr_t) p;
			iov->iov_len = extra_in;
			++iov;
		} else {
			command->indata = p;
		}
		command->inlen_alloc += extra_in;
	}

	return 0;
}

/*
 * Must be called once after a command involving attributes.  Figures out
 * where GET output data landed and sets .val pointers and .outlen
 * appropriately.  Returns 0 if all okay.
 */
int osd_command_attr_resolve(struct osd_command *command)
{
	struct attr_malloc_header *header = command->attr_malloc;
	uint8_t *p;
	uint64_t len;
	uint32_t list_len;
	int numget, numgetmulti, i;
	int ret = -EINVAL;
	struct attribute_list *attr = command->attr;
	int numattr = command->numattr;

	if (command->inlen < header->start_retrieved)
		goto unwind;

	p = header->retrieved_buf;
	len = command->inlen - header->start_retrieved;

	/*
	 * If getpage, just copy it in and done.  Assumes checking was
	 * done properly at the start.  Also error out the outlens for
	 * the non-getpage case to come later.
	 */
	numget = 0;
	numgetmulti = 0;
	for (i=0; i<numattr; i++) {
		if (attr[i].type == ATTR_GET_PAGE) {
			attr[i].outlen = len;
			if (attr[i].outlen > attr[i].len)
				attr[i].outlen = attr[i].len;
			attr[i].val = p;
			ret = 0;
			goto unwind;
		}
		if (attr[i].type == ATTR_GET) {
			attr[i].outlen = 0xffff;
			++numget;
	    	}
		if (attr[i].type == ATTR_GET_MULTI) {
			attr[i].val = &header->mr[numgetmulti];
			++numgetmulti;
		}
	}

	/*
	 * No ATTR_GET to process, not an error.
	 */
	if (numget == 0 && numgetmulti == 0) {
		ret = 0;
		goto unwind;
	}

	/*
	 * Read off the header first.
	 */
	if (len < 8)
		goto unwind;

	if ((p[0] & 0xf) == 0x9) {
		if (numgetmulti) {
			osd_error("%s: got list type 9, expecting multi",
				  __func__);
			goto unwind;
		}
	} else if ((p[0] & 0xf) == 0xf) {
		if (!numgetmulti) {
			osd_error("%s: got list type f, not expecting multi",
				  __func__);
			goto unwind;
		}
	} else {
		osd_error("%s: expecting list type 9 or f, got 0x%x",
			  __func__, p[0] & 0xf);
		goto unwind;
	}

	list_len = ntohl(&p[4]) + 8;
	if (list_len > len) {
		osd_error("%s: target says it returned %u, but really %llu",
			  __func__, list_len, llu(len));
		goto unwind;
	}
	len = list_len;
	p += 8;
	len -= 8;

	/*
	 * Process retrieved entries, figuring out where they go.
	 */
	for (;;) {
		uint64_t oid;
		uint32_t page, number;
		uint16_t item_len, pad;
		uint16_t avail_len;

		if (len < 10u + 8 * !!numgetmulti)
			break;
		if (numgetmulti) {
			oid = ntohll(&p[0]);
			p += 8;
		}
		page = ntohl(&p[0]);
		number = ntohl(&p[4]);
		item_len = ntohs(&p[8]);
		p += 10;
		len -= 10;

		for (i=0; i<numattr; i++) {
			if (!(attr[i].type == ATTR_GET
			   || attr[i].type == ATTR_GET_MULTI))
				continue;
			if (attr[i].page == page && attr[i].number == number)
				break;
		}

		if (i == numattr) {
			osd_error("%s: page %x number %x not requested",
				  __func__, page, number);
			goto unwind;
		}

		avail_len = item_len;
		if (item_len == 0xffff)
			avail_len = 0;   /* not found on target */

		/*
		 * Ran off the end of the allocated buffer?
		 */
		if (avail_len > len)
			avail_len = len;

		/*
		 * This can happen for a list of gets since the target does
		 * not know the requested size of each one, just return what
		 * we can.
		 */
		if (avail_len > attr[i].len)
			avail_len = attr[i].len;

		/* set it, even if len is zero */
		if (attr[i].type == ATTR_GET_MULTI) {
			struct attribute_get_multi_results *mr
				= attr[i].val;

			mr->oid[mr->numoid] = oid;
			mr->val[mr->numoid] = avail_len ? p : NULL;
			mr->outlen[mr->numoid] = avail_len;
			++mr->numoid;
		} else {
			attr[i].val = avail_len ? p : NULL;
			attr[i].outlen = avail_len;
		}

		if (item_len == 0xffff)
			item_len = 0;

		pad = roundup8(10 + item_len) - (10 + item_len);
		if (item_len + pad >= len)
			break;

		p += item_len + pad;
		len -= item_len + pad;
	}
	ret = 0;

unwind:
	command->outdata = header->orig_outdata;
	command->outlen = header->orig_outlen;
	command->iov_outlen = header->orig_iov_outlen;
	command->indata = header->orig_indata;
	command->inlen_alloc = header->orig_inlen_alloc;
	command->iov_inlen = header->orig_iov_inlen;
	if (command->inlen > header->orig_inlen)
		command->inlen = header->orig_inlen;
	return ret;
}

void osd_command_attr_free(struct osd_command *command)
{
	free(command->attr_malloc);
}

