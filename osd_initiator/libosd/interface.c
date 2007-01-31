/*
 * Talk to the kernel module.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <stdint.h>
#include <sys/types.h>
#include "common.h"

#include "util/util.h"

#include "interface.h"
#include "osd_hdr.h"
#include "../../kernel/suo_ioctl.h"

/*Forward declaration -- don't define in interface.h or test codes will complain*/
static int write_cdb(int fd, const uint8_t *cdb, int cdb_len,  enum data_direction dir,
		     const void *outbuf,  size_t outlen, void *inbuf, size_t inlen);


/*Functions for user codes to manipulate the character device*/
int dev_osd_open(const char *dev)
{
	int interface_fd = open(dev, O_RDWR);
	return interface_fd;
}

void dev_osd_close(int fd)
{
	close(fd);
}

/* blocking */
int dev_osd_wait_response(int fd, uint64_t *key)
{
	int ret;
	struct suo_response response;

	ret = read(fd, &response, sizeof(response));
	if (ret < 0)
		error_errno("%s: read response", __func__);
	if (ret != sizeof(response))
		error("%s: got %d bytes, expecting response %zu bytes",
		      __func__, ret, sizeof(response));
	if (key)
		*key = response.key;
	return response.error;
}

/*
 * Trying a different style.  Need sense data sometimes.
 */
int dev_osd_wait_response2(int fd, struct dev_response *devresp)
{
	int ret;
	struct suo_response response;

	ret = read(fd, &response, sizeof(response));
	if (ret < 0)
		error_errno("%s: read response", __func__);
	if (ret != sizeof(response))
		error("%s: got %d bytes, expecting response %zu bytes",
		      __func__, ret, sizeof(response));
	/* I don't particularly like this copying.  But maybe it is
	 * unavoidable.  Or we could come up with a shared interface
	 * for kernel/userlib and userlib/usercode to share.
	 */
	devresp->key = response.key;
	devresp->error = response.error;
	devresp->sense_buffer_len = response.sense_buffer_len;
	memcpy(devresp->sense_buffer, response.sense_buffer,
	       response.sense_buffer_len);
	return 0;
}

/*
 * Debugging, should go in some util library.
 */
void hexdump(uint8_t *d, size_t len)
{
	size_t offset = 0;

	while (offset < len) {
		unsigned int i, range;

		range = 8;
		if (range > len-offset)
			range = len-offset;
		printf("%4lx:", offset);
		for (i=0; i<range; i++)
			printf(" %02x", d[offset+i]);
		printf("\n");
		offset += range;
	}
}

/* description of the sense key values */
static const char *const snstext[] = {
	"No Sense",	    /* 0: There is no sense information */
	"Recovered Error",  /* 1: The last command completed successfully
				  but used error correction */
	"Not Ready",	    /* 2: The addressed target is not ready */
	"Medium Error",	    /* 3: Data error detected on the medium */
	"Hardware Error",   /* 4: Controller or device failure */
	"Illegal Request",  /* 5: Error in request */
	"Unit Attention",   /* 6: Removable medium was changed, or
				  the target has been reset, or ... */
	"Data Protect",	    /* 7: Access to the data is blocked */
	"Blank Check",	    /* 8: Reached unexpected written or unwritten
				  region of the medium */
	"Vendor Specific(9)",
	"Copy Aborted",	    /* A: COPY or COMPARE was aborted */
	"Aborted Command",  /* B: The target aborted the command */
	"Equal",	    /* C: A SEARCH DATA command found data equal */
	"Volume Overflow",  /* D: Medium full with still data to be written */
	"Miscompare",	    /* E: Source data and data on the medium
				  do not agree */
};

/*
 * Possibly debugging support.  Parse and print out interesting bits of the
 * sense (i.e. error) message returned from the target.  Maybe suck in all
 * the strings from linux/drivers/scsi/constants.c someday.  In a separate
 * file.
 */
void dev_show_sense(uint8_t *sense, int len)
{
	uint8_t code, key, asc, ascq, additional_len;
	uint8_t *info;
	uint64_t pid, oid;
	const char *keystr = "(unknown)";

	/* hexdump(sense, len); */
	if (len < 8) {
		printf("%s: sense length too short\n", __func__);
		return;
	}
	code = sense[0];
	if ((code & 0x72) != 0x72) {
		printf("%s: code 0x%02x not expected 0x72 or 0x73\n",
		       __func__, code);
		return;
	}
	key = sense[1];
	asc = sense[2];
	ascq = sense[3];
	if (key < ARRAY_SIZE(snstext))
		keystr = snstext[key];
	printf("%s: %s key 0x%02x %s asc 0x%02x ascq 0x%02x\n", __func__,
	       (code & 1) ? "deferred" : "current", key, keystr, asc, ascq);
	additional_len = sense[7];
	if (additional_len < 32) {
		printf("%s: additional len %d not enough for OSD info\n",
		       __func__, additional_len);
		return;
	}
	info = sense + 8;
	if (info[0] != 0x6) {
		printf("%s: unexpected data descriptor type 0x%02x\n",
		       __func__, info[0]);
		return;
	}
	if (info[1] != 0x1e) {
		printf("%s: invalid info length 0x%02x\n", __func__, info[1]);
		return;
	}
	/* ignore not_init and completed funcs */
	/* XXX: need these funcs from a standard interface somewhere...
	pid = ntohll(&info[16]);
	oid = ntohll(&info[16]);
	printf("%s: offending pid 0x%016lx oid 0x%016lx\n", __func__, pid, oid);
	*/
}

int dev_osd_write_nodata(int fd, const uint8_t *cdb, int cdb_len)
{
	return write_cdb(fd, cdb, cdb_len, DMA_NONE, NULL, 0, NULL, 0);
}

int dev_osd_write(int fd, const uint8_t *cdb, int cdb_len, const void *buf, size_t len)
{
	return write_cdb(fd, cdb, cdb_len, DMA_TO_DEVICE, buf, len, NULL, 0);
}

int dev_osd_read(int fd, const uint8_t *cdb, int cdb_len, void *buf, size_t len)
{
	return write_cdb(fd, cdb, cdb_len, DMA_FROM_DEVICE, NULL, 0, buf, len);
}

int dev_osd_bidir(int fd, const uint8_t *cdb, int cdb_len, const void *outbuf,
		   size_t outlen, void *inbuf, size_t inlen)
{
	error("%s: cannot do bidirectional yet", __func__);
	return write_cdb(fd, cdb, cdb_len, DMA_TO_DEVICE, outbuf, outlen,
	               inbuf, inlen);
}




/*Actually do the writing to the char dev - not for user codes*/
static int write_cdb(int fd, 
		     const uint8_t *cdb, 
		     int cdb_len, 
		     enum data_direction dir,
		     const void *outbuf, 
		     size_t outlen, 
		     void *inbuf, 
		     size_t inlen)
{
	struct suo_req req;
	int ret;

	req.data_direction = dir;
	req.cdb_len = cdb_len;
	req.cdb_buf = (uint64_t) (uintptr_t)cdb;
	req.in_data_len = (uint32_t) inlen;
	req.in_data_buf = (uint64_t) (uintptr_t) inbuf;
	req.out_data_len = (uint32_t) outlen;
	req.out_data_buf = (uint64_t) (uintptr_t) outbuf;

	info("%s: write_cdb %02x len %d inbuf %p len %zu outbuf %p len %zu",
	     __func__, cdb[0], cdb_len, inbuf, inlen, outbuf, outlen);
	ret = write(fd, &req, sizeof(req));
	if (ret < 0)
		error_errno("%s: write suo request", __func__);
	return ret;
}



/*bit twiddling functions*/
static uint32_t swab32(uint32_t d)
{
	return  (d & (uint32_t) 0x000000ffUL) << 24 |
	        (d & (uint32_t) 0x0000ff00UL) << 8  |
	        (d & (uint32_t) 0x00ff0000UL) >> 8  |
	        (d & (uint32_t) 0xff000000UL) >> 24;
} 

#if 0  /* will be needed for parsing returned values */
/*
 * Things are not aligned in the current osd2r00, but they probably
 * will be soon.  Assume 4-byte alignment though.
 */
static uint64_t ntohll_le(uint8_t *d)
{
	uint32_t d0 = swab32(*(uint32_t *) d);
	uint32_t d1 = swab32(*(uint32_t *) (d+4));

	return (uint64_t) d0 << 32 | d1;
}

static uint32_t ntohl_le(uint8_t *d)
{
	return swab32(*(uint32_t *) d);
}

static uint16_t ntohs_le(uint8_t *d)
{
	uint16_t x = *(uint16_t *) d;

	return (x & (uint16_t) 0x00ffU) << 8 |
	       (x & (uint16_t) 0xff00U) >> 8;
}
#endif

static void set_htonll_le(uint8_t *x, uint64_t val)
{
	uint32_t *xw = (uint32_t *) x;

	xw[0] = swab32((val & (uint64_t) 0xffffffff00000000ULL) >> 32);
	xw[1] = swab32((val & (uint64_t) 0x00000000ffffffffULL));
}

static void set_htonl_le(uint8_t *x, uint32_t val)
{
	uint32_t *xw = (uint32_t *) x;

	*xw = swab32(val);
}

static void set_htons_le(uint8_t *x, uint16_t val)
{
	uint16_t *xh = (uint16_t *) x;

	*xh = (val & (uint16_t) 0x00ffU) << 8 |
	      (val & (uint16_t) 0xff00U) >> 8; 
}

/* some day deal with the big-endian versions */
#define     ntohs      ntohs_le
#define     ntohl      ntohl_le
#define     ntohll     ntohll_le
#define set_htons  set_htons_le
#define set_htonl  set_htonl_le
#define set_htonll set_htonll_le


static void set_action(uint8_t *cdb, uint16_t command)
{
	cdb[8] = (command & 0xff00U) >> 8;
	cdb[9] = (command & 0x00ffU);
}

/*
 * These functions take a cdb of the appropriate size (200 bytes),
 * and the arguments needed for the particular command.  They marshal
 * the arguments but do not submit the command.
 */
int set_cdb_osd_append(uint8_t *cdb, uint64_t pid, uint64_t oid, uint64_t len)
{
	debug(__func__);
	set_action(cdb, OSD_APPEND);
	set_htonll(&cdb[16], pid);
	set_htonll(&cdb[24], oid);
	set_htonll(&cdb[36], len);
	return 0;
}


int set_cdb_osd_create(uint8_t *cdb, uint64_t pid, uint64_t requested_oid, uint16_t num)
{
	debug(__func__);
	set_action(cdb, OSD_CREATE);
	set_htonll(&cdb[16], pid);
	set_htonll(&cdb[24], requested_oid);
	set_htons(&cdb[36], num);
	return 0;
}


int set_cdb_osd_create_and_write(uint8_t *cdb, uint64_t pid, uint64_t requested_oid,
                         uint64_t len, uint64_t offset)
{
	debug(__func__);
	set_action(cdb, OSD_CREATE_AND_WRITE);
	set_htonll(&cdb[16], pid);
	set_htonll(&cdb[24], requested_oid);
	set_htonll(&cdb[36], len);
	set_htonll(&cdb[44], offset);
	return 0;
}


int set_cdb_osd_create_collection(uint8_t *cdb, uint64_t pid, uint64_t requested_cid)
{
	debug(__func__);
	set_action(cdb, OSD_CREATE_COLLECTION);
	set_htonll(&cdb[16], pid);
	set_htonll(&cdb[24], requested_cid);
	return 0;
}


int set_cdb_osd_create_partition(uint8_t *cdb, uint64_t requested_pid)
{
	debug(__func__);
	set_action(cdb, OSD_CREATE_PARTITION);
	set_htonll(&cdb[16], requested_pid);
	return 0;
}


int set_cdb_osd_flush(uint8_t *cdb, uint64_t pid, uint64_t oid, int flush_scope)
{
	debug(__func__);
	set_action(cdb, OSD_FLUSH);
	set_htonll(&cdb[16], pid);
	set_htonll(&cdb[24], oid);
	cdb[10] = (cdb[10] & ~0x3) | flush_scope;
	return 0;
}


int set_cdb_osd_flush_collection(uint8_t *cdb, uint64_t pid, uint64_t cid,
                         int flush_scope)
{
	debug(__func__);
	set_action(cdb, OSD_FLUSH_COLLECTION);
	set_htonll(&cdb[16], pid);
	set_htonll(&cdb[24], cid);
	cdb[10] = (cdb[10] & ~0x3) | flush_scope;
	return 0;
}


int set_cdb_osd_flush_osd(uint8_t *cdb, int flush_scope)
{
	debug(__func__);
	set_action(cdb, OSD_FLUSH_OSD);
	cdb[10] = (cdb[10] & ~0x3) | flush_scope;
	return 0;
}


int set_cdb_osd_flush_partition(uint8_t *cdb, uint64_t pid, int flush_scope)
{
	debug(__func__);
	set_action(cdb, OSD_FLUSH_PARTITION);
	set_htonll(&cdb[16], pid);
	cdb[10] = (cdb[10] & ~0x3) | flush_scope;
	return 0;
}

/*
 * Destroy the db and start over again.
 */
int set_cdb_osd_format_osd(uint8_t *cdb, uint64_t capacity)
{
	debug(__func__);
	set_action(cdb, OSD_FORMAT_OSD);
	set_htonll(&cdb[36], capacity);
	return 0;
}


int set_cdb_osd_get_attributes(uint8_t *cdb, uint64_t pid, uint64_t oid)
{
	debug(__func__);
	set_action(cdb, OSD_GET_ATTRIBUTES);
	set_htonll(&cdb[16], pid);
	set_htonll(&cdb[24], oid);
	return 0;
}


int set_cdb_osd_get_member_attributes(uint8_t *cdb, uint64_t pid, uint64_t cid) /*section in spec?*/
{
	debug(__func__);
	set_action(cdb, OSD_GET_MEMBER_ATTRIBUTES);
	set_htonll(&cdb[16], pid);
	set_htonll(&cdb[24], cid);
	return 0;
}


int set_cdb_osd_list(uint8_t *cdb, uint64_t pid, uint32_t list_id, uint64_t alloc_len,
             uint64_t initial_oid)
{
	debug(__func__);
	set_action(cdb, OSD_LIST);
	set_htonll(&cdb[16], pid);
	set_htonl(&cdb[32], list_id);
	set_htonll(&cdb[36], alloc_len);
	set_htonll(&cdb[44], initial_oid);
	return 0;
}


int set_cdb_osd_list_collection(uint8_t *cdb, uint64_t pid, uint64_t cid,  /*section in spec?*/
                        uint32_t list_id, uint64_t alloc_len,
			uint64_t initial_oid)
{
	debug(__func__);
	set_action(cdb, OSD_LIST_COLLECTION);
	set_htonll(&cdb[16], pid);
	set_htonll(&cdb[24], cid);
	set_htonl(&cdb[32], list_id);
	set_htonll(&cdb[36], alloc_len);
	set_htonll(&cdb[44], initial_oid);
	return 0;
}

int set_cdb_osd_perform_scsi_command(uint8_t *cdb)
{
	error("%s: unimplemented", __func__);
	return 1;
}

int set_cdb_osd_perform_task_mgmt_func(uint8_t *cdb)
{
	error("%s: unimplemented", __func__);
	return 1;
}

int set_cdb_osd_query(uint8_t *cdb, uint64_t pid, uint64_t cid, uint32_t query_len,    /*section in spec?*/
              uint64_t alloc_len)
{
	debug(__func__);
	set_action(cdb, OSD_QUERY);
	set_htonll(&cdb[16], pid);
	set_htonll(&cdb[24], cid);
	set_htonl(&cdb[32], query_len);
	set_htonll(&cdb[36], alloc_len);
	return 0;
}


int set_cdb_osd_read(uint8_t *cdb, uint64_t pid, uint64_t oid, uint64_t len,
	     uint64_t offset)
{
	debug(__func__);
	set_action(cdb, OSD_READ);
	set_htonll(&cdb[16], pid);
	set_htonll(&cdb[24], oid);
	set_htonll(&cdb[36], len);
	set_htonll(&cdb[44], offset);
	return 0;
}


int set_cdb_osd_remove(uint8_t *cdb, uint64_t pid, uint64_t oid)
{
	debug(__func__);
	set_action(cdb, OSD_REMOVE);
	set_htonll(&cdb[16], pid);
	set_htonll(&cdb[24], oid);
	return 0;
}


int set_cdb_osd_remove_collection(uint8_t *cdb, uint64_t pid, uint64_t cid)
{
	debug(__func__);
	set_action(cdb, OSD_REMOVE_COLLECTION);
	set_htonll(&cdb[16], pid);
	set_htonll(&cdb[24], cid);
	return 0;
}


int set_cdb_osd_remove_member_objects(uint8_t *cdb, uint64_t pid, uint64_t cid) /*section in spec?*/
{
	debug(__func__);
	set_action(cdb, OSD_REMOVE_MEMBER_OBJECTS);
	set_htonll(&cdb[16], pid);
	set_htonll(&cdb[24], cid);
	return 0;
}


int set_cdb_osd_remove_partition(uint8_t *cdb, uint64_t pid)
{
	debug(__func__);
	set_action(cdb, OSD_REMOVE_PARTITION);
	set_htonll(&cdb[16], pid);
	return 0;
}

int set_cdb_osd_set_attributes(uint8_t *cdb, uint64_t pid, uint64_t oid)
{
	debug(__func__);
	set_action(cdb, OSD_SET_ATTRIBUTES);
	set_htonll(&cdb[16], pid);
	set_htonll(&cdb[24], oid);
	return 0;
}


int set_cdb_osd_set_key(uint8_t *cdb, int key_to_set, uint64_t pid, uint64_t key,
                uint8_t seed[20])
{
	debug(__func__);
	set_action(cdb, OSD_SET_KEY);
	cdb[11] = (cdb[11] & ~0x3) | key_to_set;
	set_htonll(&cdb[16], pid);
	set_htonll(&cdb[24], key);
	memcpy(&cdb[32], seed, 20);
	return 0;
}


int set_cdb_osd_set_master_key(uint8_t *cdb, int dh_step, uint64_t key,
                       uint32_t param_len, uint32_t alloc_len)
{
	debug(__func__);
	set_action(cdb, OSD_SET_KEY);
	cdb[11] = (cdb[11] & ~0x3) | dh_step;
	set_htonll(&cdb[24], key);
	set_htonl(&cdb[32], param_len);
	set_htonl(&cdb[36], alloc_len);
	return 0;
}


int set_cdb_osd_set_member_attributes(uint8_t *cdb, uint64_t pid, uint64_t cid)
{
	debug(__func__);
	set_action(cdb, OSD_SET_MEMBER_ATTRIBUTES);
	set_htonll(&cdb[16], pid);
	set_htonll(&cdb[24], cid);
	return 0;
}


int set_cdb_osd_write(uint8_t *cdb, uint64_t pid, uint64_t oid, uint64_t len,
	      uint64_t offset)
{
	debug(__func__);
	set_action(cdb, OSD_WRITE);
	set_htonll(&cdb[16], pid);
	set_htonll(&cdb[24], oid);
	set_htonll(&cdb[36], len);
	set_htonll(&cdb[44], offset);
	return 0;
}

