#include <string.h>
#include <errno.h>
#include <stdint.h>

#include "util/util.h"
#include "command.h"
#include "osd_cmds.h"

#define VARLEN_CDB 0x7f
#define TIMESTAMP_OFF 0x7f

/* cdb initialization / manipulation functions */

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
	command->cdb[12] = TIMESTAMP_OFF;
}

/*
 * Non-varlen commands.
 */
int osd_command_set_inquiry(struct osd_command *command, uint8_t outlen)
{
	memset(command, 0, sizeof(*command));
	command->cdb_len = 6;
	memset(command->cdb, 0, 6);
	command->cdb[0] = INQUIRY;
	command->cdb[4] = outlen;
	return 0;
}

/*
 * These functions take a cdb of the appropriate size (200 bytes),
 * and the arguments needed for the particular command.  They marshall
 * the arguments but do not submit the command.
 */     
int osd_command_set_append(struct osd_command *command, uint64_t pid, uint64_t oid, uint64_t len)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_APPEND);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], oid);
        set_htonll(&command->cdb[36], len);
        return 0;
}


int osd_command_set_create(struct osd_command *command, uint64_t pid, uint64_t requested_oid, uint16_t num_user_objects)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_CREATE);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], requested_oid);
        set_htons(&command->cdb[36], num_user_objects);
        return 0;
}


int osd_command_set_create_and_write(struct osd_command *command, uint64_t pid, uint64_t requested_oid,
                         uint64_t len, uint64_t offset)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_CREATE_AND_WRITE);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], requested_oid);
        set_htonll(&command->cdb[36], len);
        set_htonll(&command->cdb[44], offset);
        return 0;
}


int osd_command_set_create_collection(struct osd_command *command, uint64_t pid, uint64_t requested_cid)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_CREATE_COLLECTION);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], requested_cid);
        return 0;
}

int osd_command_set_create_partition(struct osd_command *command, uint64_t requested_pid)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_CREATE_PARTITION);
        set_htonll(&command->cdb[16], requested_pid);
        return 0;
}


int osd_command_set_flush(struct osd_command *command, uint64_t pid, uint64_t oid, int flush_scope)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_FLUSH);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], oid);
        command->cdb[10] = (command->cdb[10] & ~0x3) | flush_scope;
        return 0;
}


int osd_command_set_flush_collection(struct osd_command *command, uint64_t pid, uint64_t cid,
                         int flush_scope)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_FLUSH_COLLECTION);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], cid);
        command->cdb[10] = (command->cdb[10] & ~0x3) | flush_scope;
        return 0;
}


int osd_command_set_flush_osd(struct osd_command *command, int flush_scope)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_FLUSH_OSD);
        command->cdb[10] = (command->cdb[10] & ~0x3) | flush_scope;
        return 0;
}


int osd_command_set_flush_partition(struct osd_command *command, uint64_t pid, int flush_scope)
{
        osd_debug(__func__);
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
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_FORMAT_OSD);
        set_htonll(&command->cdb[36], capacity);
        return 0;
}


int osd_command_set_get_attributes(struct osd_command *command, uint64_t pid, uint64_t oid)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_GET_ATTRIBUTES);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], oid);
        return 0;
}


int osd_command_set_get_member_attributes(struct osd_command *command, uint64_t pid, uint64_t cid) /*section in spec?*/
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_GET_MEMBER_ATTRIBUTES);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], cid);
        return 0;
}


int osd_command_set_list(struct osd_command *command, uint64_t pid, uint32_t list_id, uint64_t alloc_len,
             uint64_t initial_oid)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_LIST);
        set_htonll(&command->cdb[16], pid);
        set_htonl(&command->cdb[32], list_id);
        set_htonll(&command->cdb[36], alloc_len);
        set_htonll(&command->cdb[44], initial_oid);
        return 0;
}

int osd_command_set_list_collection(struct osd_command *command, uint64_t pid, uint64_t cid,  /*section in spec?*/
                        uint32_t list_id, uint64_t alloc_len,
                        uint64_t initial_oid)
{
        osd_debug(__func__);
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

int osd_command_set_query(struct osd_command *command, uint64_t pid, uint64_t cid, uint32_t query_len,    /*section in spec?*/
              uint64_t alloc_len)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_QUERY);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], cid);
        set_htonl(&command->cdb[32], query_len);
        set_htonll(&command->cdb[36], alloc_len);
        return 0;
}

int osd_command_set_read(struct osd_command *command, uint64_t pid, uint64_t oid, uint64_t len,
             uint64_t offset)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_READ);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], oid);
        set_htonll(&command->cdb[36], len);
        set_htonll(&command->cdb[44], offset);
        return 0;
}


int osd_command_set_remove(struct osd_command *command, uint64_t pid, uint64_t oid)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_REMOVE);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], oid);
        return 0;
}


int osd_command_set_remove_collection(struct osd_command *command, uint64_t pid, uint64_t cid)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_REMOVE_COLLECTION);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], cid);
        return 0;
}


int osd_command_set_remove_member_objects(struct osd_command *command, uint64_t pid, uint64_t cid) /*section in spec?*/
{
        varlen_cdb_init(command, OSD_REMOVE_MEMBER_OBJECTS);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], cid);
        return 0;
}


int osd_command_set_remove_partition(struct osd_command *command, uint64_t pid)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_REMOVE_PARTITION);
        set_htonll(&command->cdb[16], pid);
        return 0;
}


int osd_command_set_set_attributes(struct osd_command *command, uint64_t pid, uint64_t oid)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_SET_ATTRIBUTES);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], oid);
        return 0;
}


int osd_command_set_set_key(struct osd_command *command, int key_to_set, uint64_t pid, uint64_t key,
                uint8_t seed[20])
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_SET_KEY);
        command->cdb[11] = (command->cdb[11] & ~0x3) | key_to_set;
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], key);
        memcpy(&command->cdb[32], seed, 20);
        return 0;
}


int osd_command_set_set_master_key(struct osd_command *command, int dh_step, uint64_t key,
                       uint32_t param_len, uint32_t alloc_len)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_SET_KEY);
        command->cdb[11] = (command->cdb[11] & ~0x3) | dh_step;
        set_htonll(&command->cdb[24], key);
        set_htonl(&command->cdb[32], param_len);
        set_htonl(&command->cdb[36], alloc_len);
        return 0;
}


int osd_command_set_set_member_attributes(struct osd_command *command, uint64_t pid, uint64_t cid)
{
        osd_debug(__func__);
        varlen_cdb_init(command, OSD_SET_MEMBER_ATTRIBUTES);
        set_htonll(&command->cdb[16], pid);
        set_htonll(&command->cdb[24], cid);
        return 0;
}


int osd_command_set_write(struct osd_command *command, uint64_t pid, uint64_t oid, uint64_t len,
              uint64_t offset)
{
        osd_debug(__func__);
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
	/* optional new outiov and iniov appear next */
	/* then buffer space as described below */
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
                           struct attribute_list *attrs, int num)
{
	int use_getpage;
	int numget, numgetpage, numgetmulti, numset;
	uint32_t getsize, getpagesize, getmultisize, setsize;
	struct attr_malloc_header *header;
	struct bsg_iovec *iov;
	uint8_t *p;
	int i;
	int neediov;
	uint64_t extra;

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

	if (num == 0)
		return 0;

	/*
	 * Figure out what operations to do.  That will lead to constraints
	 * on the choice of getpage/setone versus getlist/setlist format.
	 */
	numget = numgetpage = numgetmulti = numset = 0;
	getsize = getpagesize = getmultisize = setsize = 0;
	for (i=0; i<num; i++) {
		if (attrs[i].type == ATTR_GET) {
			++numget;
			getsize += roundup8(10 + attrs[i].len);
		} else if (attrs[i].type == ATTR_GET_PAGE) {
			++numgetpage;
			getpagesize += attrs[i].len;  /* no round */
		} else if (attrs[i].type == ATTR_GET_MULTI) {
			++numgetmulti;
			getmultisize += roundup8(18 + attrs[i].len);
		} else if (attrs[i].type == ATTR_SET) {
			++numset;
			setsize += roundup8(10 + attrs[i].len);
		} else {
			osd_error("%s: invalid attribute type %d", __func__,
			          attrs[i].type);
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
			uint32_t num_user_objects = ntohs(&command->cdb[36]);
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
			getmultisize *= num_user_objects;
		}
		if (numget == 0 && numgetmulti == 0 && numset == 1)
			use_getpage = 1;  /* simpler for a single set */
		else
			use_getpage = 0;  /* build list(s) */
	}

	/*
	 * Calculate sizes of padding and attribute areas:
	 * 	outdata:
	 * 		padding between real outdata and getlist
	 *	 	getlist
	 *	 	padding between getlist and setlist
	 *	 	setlist (including values)
	 *	indata:
	 *	 	padding between real indata and retrieved results area
	 *	 	retrieved results area
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
	if (numset && !use_getpage) {
		start_setlist = next_offset(start_getlist + size_getlist);
		size_pad_getlist = start_setlist
				 - (start_getlist + size_getlist);
		size_setlist = 8 + setsize;  /* already rounded */
	}

	end_indata = command->inlen;
	start_retrieved = next_offset(end_indata);
	size_pad_indata = start_retrieved - end_indata;

	size_retrieved = 0;
	if (numget)
		size_retrieved = 8 + getsize;  /* already rounded */
	if (numgetpage)
		size_retrieved = getpagesize;  /* no rounding */
	if (numgetmulti)
		size_retrieved = 8 + getmultisize;  /* already scaled/rounded */

	/*
	 * Allocate space for the getlist, setlist, retrieved areas.
	 * Also include a header to remember the original iovs or
	 * indata/outdata, and space for the new iovs.
	 */
	neediov = 0;
	if (command->iov_outlen)
		neediov += command->iov_outlen + 1;
	if (command->iov_inlen)
		neediov += command->iov_inlen + 1;

	command->attr_malloc = Malloc(sizeof(*header)
				    + neediov * sizeof(*iov)
				    + size_pad_outdata + size_getlist
				    + size_pad_getlist + size_setlist
				    + size_pad_indata + size_retrieved);
	if (!command->attr_malloc)
		return 1;

	header = command->attr_malloc;
	iov = (void *) &header[1];
	p = (uint8_t *) &iov[neediov];

	/* set pad spaces to known pattern, for debugging */
	memset(p, 0x6b, size_pad_outdata);
	p += size_pad_outdata;

	/*
	 * Build the getlist.
	 */
	if (numget || numgetmulti) {
		uint8_t *q = p + 8;
		for (i=0; i<num; i++) {
			if (!(attrs[i].type == ATTR_GET_MULTI
			   || attrs[i].type == ATTR_GET))
				continue;
			set_htonl(&q[0], attrs[i].page);
			set_htonl(&q[4], attrs[i].number);
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
	 * Build the setlist.
	 */
	if (numset && !use_getpage) {
		uint8_t *q = p + 8;
		for (i=0; i<num; i++) {
			int len;
			if (attrs[i].type != ATTR_SET)
				continue;
			set_htonl(&q[0], attrs[i].page);
			set_htonl(&q[4], attrs[i].number);
			set_htons(&q[8], attrs[i].len);
			memcpy(&q[10], attrs[i].val, attrs[i].len);
			len = 10 + attrs[i].len;
			q += len;
			while (len & 7) {
				*q++ = 0;
				++len;
			}
		}
		p[0] = 0x9;
		p[1] = p[2] = p[3] = 0;
		set_htonl(&p[4], setsize);
		p += size_setlist;
	}
	/* no padding at the end of the setlist */

	/* Now we are at the beginning of the retrieved area */
	memset(p, 0x6b, size_pad_indata);
	p += size_pad_indata;

	/* Set it to something different, which will be overwritten. */
	header->retrieved_buf = p;
	header->start_retrieved = start_retrieved;
	memset(p, 0x7c, size_retrieved);

	/*
	 * Update the CDB bits.
	 */
	if (use_getpage) {
		/* page format */
		command->cdb[11] = command->cdb[11] | (2 << 4);
		for (i=0; i<num; i++) {
			if (attrs[i].type == ATTR_GET_PAGE)
				set_htonl(&command->cdb[52], attrs[i].page);
			if (attrs[i].type == ATTR_SET) {
				set_htonl(&command->cdb[64], attrs[i].page);
				set_htonl(&command->cdb[68], attrs[i].number);
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
	extra = size_pad_outdata + size_getlist
	      + size_pad_getlist + size_setlist;
	if (extra) {
		command->outlen += extra;
		if (command->iov_outlen) {
			memcpy(iov, command->outdata,
			       command->iov_outlen * sizeof(*iov));
			iov[command->iov_outlen].iov_base = (uintptr_t) p;
			iov[command->iov_outlen].iov_len = extra;
			++command->iov_outlen;
			iov += command->iov_outlen;
		} else
			command->outdata = p;
	}

	p += extra;
	header->orig_indata = command->indata;
	header->orig_inlen_alloc = command->inlen_alloc;
	header->orig_inlen = command->inlen;
	header->orig_iov_inlen = command->iov_inlen;
	extra = size_pad_indata + size_retrieved;
	if (extra) {
		command->inlen += extra;
		command->inlen_alloc += extra;
		if (command->iov_inlen) {
			memcpy(iov, command->indata,
			       command->iov_inlen * sizeof(*iov));
			iov[command->iov_inlen].iov_base = (uintptr_t) p;
			iov[command->iov_inlen].iov_len = extra;
			++command->iov_inlen;
		} else
			command->indata = p;
	}

	return 0;
}

/*
 * Must be called once after a command involving attributes.  Puts output
 * data from GET into the right locations and frees data structures.
 * Returns 0 if all okay.  Assumes that target returns something reasonable,
 * e.g. not multiple copies of the same attribute.
 */
int osd_command_attr_resolve(struct osd_command *command,
			     struct attribute_list *attrs, int num)
{
	struct attr_malloc_header *header = command->attr_malloc;
	uint8_t *p;
	uint64_t len;
	uint32_t list_len;
	int getmulti, i;
	int ret = -EINVAL;

	if (command->inlen < header->start_retrieved)
		goto unwind;

	p = header->retrieved_buf;
	len = command->inlen - header->start_retrieved;

	/*
	 * If getpage, just copy it in and done.  Assumes checking was
	 * done properly at the start.  Also error out the outlens for
	 * the non-getpage case to come later.
	 */
	for (i=0; i<num; i++) {
		if (attrs[i].type == ATTR_GET_PAGE) {
			attrs[i].outlen = len;
			if (attrs[i].outlen > attrs[i].len)
				attrs[i].outlen = attrs[i].len;
			memcpy(attrs[i].val, p, attrs[i].outlen);
			ret = 0;
			goto unwind;
		}
		if (attrs[i].type == ATTR_GET
		 || attrs[i].type == ATTR_GET_MULTI)
			attrs[i].outlen = 0xffff;
	}

	/*
	 * Read off the header first.
	 */
	if (len < 8)
		goto unwind;

	if ((p[0] & 0xf) == 0x9) {
		getmulti = 0;
	} else if ((p[0] & 0xf) == 0xf) {
		getmulti = 1;
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

		if (len < 10u + 8 * getmulti)
			break;
		if (getmulti) {
			oid = ntohll(&p[0]);
			p += 8;
		}
		page = ntohl(&p[0]);
		number = ntohl(&p[4]);
		item_len = ntohs(&p[8]);
		p += 10;
		len -= 10;

		for (i=0; i<num; i++) {
			if (!(attrs[i].type == ATTR_GET
			   || attrs[i].type == ATTR_GET_MULTI))
			   	continue;
			if (attrs[i].page == page && attrs[i].number == number)
				break;
		}

		if (i == num) {
			osd_error("%s: page %x number %x not requested",
				  __func__, page, number);
			goto unwind;
		}

		avail_len = item_len;
		if (avail_len > len)
			avail_len = len;

		if (attrs[i].type == ATTR_GET_MULTI) {
			uint16_t tocopy = item_len;
			uint16_t curoff = attrs[i].outlen;

			if (curoff == 0xffff)
				curoff = 0;  /* first one */

			/* append to the list of output values, but do
			 * not copy in partial values */
			if (curoff + item_len > attrs[i].len)
				tocopy = 0;
			if (item_len > avail_len)
				tocopy = 0;
			if (tocopy) {
				memcpy((uint8_t *) attrs[i].val
				       + attrs[i].outlen, p, tocopy);
				attrs[i].outlen = curoff + tocopy;
			}
		} else {
			uint16_t tocopy = avail_len;

			/* This can happen for a list of gets since the target
			 * does not know the requested size of each one, just
			 * return what we can. */
			if (tocopy > attrs[i].len)
				tocopy = attrs[i].len;

			memcpy(attrs[i].val, p, tocopy);
			attrs[i].outlen = tocopy;
		}

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
	free(header);
	return ret;
}

