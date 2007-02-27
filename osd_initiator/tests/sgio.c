/*
 * Test the use of the existing SG_IO interface to transport OSD commands.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/types.h>

#include <scsi/scsi.h>
#include <scsi/sg.h>

#include "util/util.h"
#include "kernel_interface.h"
#include "user_interface_sgio.h"
#include "diskinfo.h"
#include "cdb_manip.h"

static const uint64_t PID = 0x10000LLU;
static const uint64_t OID = 0x10003LLU;
static const uint64_t CID = 0x10006LLU;
static const uint32_t LIST_ID = 0x10009LLU;
static const uint64_t PAGE = 0; 
static const uint16_t NUM_USER_OBJ = 1;
static const uint64_t OFFSET = 0;
static const int FLUSH_SCOPE = 2; /* Flush everything */
static const int OBJ_CAPACITY = 1<<30; /* 1 GB */
static const char WRITEDATA[] = "Write some data";
static const char WRITEDATA2[] = "Test #2";
static const char WRITEDATA3[] = "write data 3";

struct attribute_id {
	uint32_t page;
	uint32_t number;
	uint16_t len;
};

static int osd_command_get_attributes(struct osd_command *command,
                                      uint64_t pid, uint64_t oid)
{
	memset(command, 0, sizeof(*command));
	set_cdb_osd_get_attributes(command->cdb, pid, oid);
	command->cdb_len = OSD_CDB_SIZE;
	command->sense_len = OSD_MAX_SENSE;
	return 0;
}

/*
 * Assume an empty command with no data, no retrieved offset, etc.
 * Build an attributes list, including allocating in and out space,
 * and alter the CDB.
 */
static int osd_command_build_attr_list(struct osd_command *command,
                                       struct attribute_id *attrs, int num)
{
    const uint32_t list_offset = 0, retrieved_offset = 0;
    uint32_t list_len, alloc_len;
    uint8_t *attr_list;
    int i;

    /* must fit in 2-byte list length field */
    if (num * 8 >= (1 << 16))
	return -EOVERFLOW;

    /*
     * Build the list that requests the attributes
     */
    list_len = 4 + num * 8;
    command->outlen = list_len;
    attr_list = malloc(command->inlen_alloc);
    if (!attr_list)
    	return -ENOMEM;
    command->outdata = attr_list;

    attr_list[0] = 0x1;  /* list type: retrieve attributes */
    attr_list[1] = 0;
    set_htons(&attr_list[2], num*8);
    for (i=0; i<num; i++) {
	set_htonl(&attr_list[4 + i*8 + 0], attrs[i].page);
	set_htonl(&attr_list[4 + i*8 + 4], attrs[i].number);
    }

    /*
     * Allocate space for where they will end up when returned.  Apparently
     * no padding here, just squeezed together exactly, with a 10-byte
     * header on each one.  Whole thing is preceded by the usual 4-byte
     * Table 126 list header.
     */
    alloc_len = 4;
    for (i=0; i<num; i++)
	alloc_len += 10 + attrs[i].len;

    command->inlen_alloc = alloc_len;
    command->indata = malloc(command->inlen_alloc);
    if (!command->indata)
	return -ENOMEM;

    /* Set the CDB bits to point appropriately. */
    set_cdb_get_attr_list(command->cdb, list_len, list_offset, alloc_len,
                          retrieved_offset);

    return 0;
}


static int bidi_test(int fd)
{
	int ret;
	struct osd_command command;
	uint64_t logical_length;
	struct attribute_id id = {
	    .page = 0x1,
	    .number = 0x82,  /* logical length (not used capacity) */
	    .len = sizeof(uint64_t),
	};
	uint8_t *p;

	ret = osd_command_get_attributes(&command, PID, OID);
	if (ret) {
		osd_error_xerrno(ret, "%s: get_attributes failed", __func__);
		return 1;
	}
	ret = osd_command_build_attr_list(&command, &id, 1);
	if (ret) {
		osd_error_xerrno(ret, "%s: build_attr_list failed", __func__);
		return 1;
	}
	memset(command.indata, 0xaa, command.inlen_alloc);
	ret = osd_sgio_submit_and_wait(fd, &command);
	if (ret) {
		osd_error_xerrno(ret, "%s: submit failed", __func__);
		return 1;
	}
	printf("%s: status %u sense len %u inlen %zu\n", __func__,
	       command.status, command.sense_len, command.inlen);

	/* verify retrieved list */
	p = command.indata;
	osd_hexdump(command.indata, command.inlen_alloc);
	if (command.inlen != 4 + 10u + id.len) {
		osd_error("%s: expecting %u bytes, got %zu, [0] = %02x",
		          __func__, 4 + 10u + id.len, command.inlen, p[0]);
		return 1;
	}
	if ((p[0] & 0xf) != 0x9) {
		osd_error("%s: expecting list type 9, got 0x%x", __func__,
		          p[0] & 0xf);
		return 1;
	}
	if (ntohs(&p[2]) != command.inlen - 4) {
		osd_error("%s: expecting list length %zu, got %u", __func__,
		          command.inlen - 4, ntohs(&p[2]));
		return 1;
	}
	if (ntohl(&p[4]) != id.page) {
		osd_error("%s: expecting page %x, got %x", __func__,
		          id.page, ntohl(&p[4]));
		return 1;
	}
	if (ntohl(&p[8]) != id.number) {
		osd_error("%s: expecting number %x, got %x", __func__,
		          id.page, ntohl(&p[8]));
		return 1;
	}
	if (ntohs(&p[12]) != id.len) {
		osd_error("%s: expecting length %u, got %u", __func__,
		          id.len, ntohs(&p[12]));
		return 1;
	}

	logical_length = ntohll(&p[14]);
	printf("%s: logical length 0x%016llx\n", __func__, llu(logical_length));
	free(command.indata);
	free((void *)(uintptr_t) command.outdata);
	return 0;
}

int main(int argc, char *argv[])
{
	int fd, ret, num_drives, i;
	struct osd_drive_description *drives;

	osd_set_progname(argc, argv); 

	ret = osd_get_drive_list(&drives, &num_drives);
	if (ret < 0) {
		osd_error("%s: get drive error", __func__);
		return 1;
	}
	if (num_drives == 0) {
		osd_error("%s: no drives", __func__);
		return 1;
	}
	
	for (i=0; i<num_drives; i++) {

		printf("%s: drive %s name %s\n", progname, drives[i].chardev,
		       drives[i].targetname);
		fd = open(drives[i].chardev, O_RDWR);
		if (fd < 0) {
			osd_error_errno("%s: open %s", __func__, drives[i].chardev);
			return 1;
		}

		inquiry_sgio(fd);

		/*
		 * Clean the slate and make one partition.
		 */
		format_osd_sgio(fd, OBJ_CAPACITY); 
		create_partition_sgio(fd, PID);


#if 0           /* These are all supposed to fail, for various reasons. */
		write_osd_sgio(fd, PID, OID, WRITEDATA, OFFSET);
		flush_osd_sgio(fd, FLUSH_SCOPE);
#endif

		
#if 0		/* Basic read / write seems to work */
		create_osd_sgio(fd, PID, OID, NUM_USER_OBJ+2);
		remove_osd_sgio(fd, PID, OID+1);

		write_osd_sgio(fd, PID, OID, WRITEDATA, OFFSET);
		read_osd_sgio(fd, PID, OID, OFFSET);
		write_osd_sgio(fd, PID, OID+2, WRITEDATA2, OFFSET);
		read_osd_sgio(fd, PID, OID+2, OFFSET);
	
		read_osd_sgio(fd, PID, OID, OFFSET);

#endif

#if 0           /* Testing bidirectional operations. */
		bidi_test(fd);
#endif


#if 1		/* Testing stuff */
		create_osd_sgio(fd, PID, OID, NUM_USER_OBJ+3);
		write_osd_sgio(fd, PID, OID, WRITEDATA, OFFSET);
		write_osd_sgio(fd, PID, OID+1, WRITEDATA2, OFFSET);
		write_osd_sgio(fd, PID, OID+2, WRITEDATA3, OFFSET);
		get_attributes_sgio(fd, PID, OID);
		object_list_sgio(fd, PID, LIST_ID, OID);
		read_osd_sgio(fd, PID, OID+1, OFFSET);
		get_attribute_page_sgio(fd, PAGE, OFFSET);
#endif
		close(fd);
	}

	osd_free_drive_list(drives, num_drives);

	return 0;
}
