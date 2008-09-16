/*
 * Test the use of the existing SG_IO interface to transport OSD commands.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "util/osd-util.h"
#include "command.h"
#include "device.h"
#include "drivelist.h"
#include "sync.h"

static const uint64_t PID = 0x10000LLU;
static const uint64_t OID = 0x10000LLU;
static const uint64_t PAGE = 0; 
static const uint16_t NUM_USER_OBJ = 1;
static const uint64_t OFFSET = 0;
static const int FLUSH_SCOPE = 2; /* Flush everything */
static const int OBJ_CAPACITY = 1<<30; /* 1 GB */
static const uint8_t WRITEDATA[] = "Write some data";
static const uint8_t WRITEDATA2[] = "Test #2";
static const uint8_t WRITEDATA3[] = "write data 3";

static int bidi_test(int fd, uint64_t pid, uint64_t oid)
{
	int ret;
	struct osd_command command;
	struct attribute_list *attr, attr_proto = {
	    .page = 0x1,
	    .number = 0x82,  /* logical length (not used capacity) */
	    .len = sizeof(uint64_t),
	};

	osd_info(__func__);
	ret = osd_command_set_get_attributes(&command, pid, oid);
	if (ret) {
		osd_error_xerrno(ret, "%s: get_attributes failed", __func__);
		printf("\n");
		return 1;
	}
	ret = osd_command_attr_build(&command, &attr_proto, 1);
	if (ret) {
		osd_error_xerrno(ret, "%s: attr_build failed", __func__);
		printf("\n");
		return 1;
	}
	memset(command.indata, 0xaa, command.inlen_alloc);
	ret = osd_submit_and_wait(fd, &command);
	if (ret) {
		osd_error_xerrno(ret, "%s: submit failed", __func__);
		printf("\n");
		return 1;
	}
	printf("%s: status %u sense len %u inlen %zu\n", __func__,
	       command.status, command.sense_len, command.inlen);

	/* verify retrieved list */
	osd_hexdump(command.indata, command.inlen_alloc);
	ret = osd_command_attr_resolve(&command);
	if (ret) {
		osd_error("%s: attr_resolve failed", __func__);
		printf("\n");
		return 1;
	}
	attr = command.attr;

	if (attr->outlen != attr->len)
		osd_error("%s: short attr outlen %d", __func__,
		          attr->outlen);

	printf("%s: logical length 0x%016llx\n\n", __func__,
	       llu(get_ntohll(attr->val)));

	osd_command_attr_free(&command);
	return 0;
}

static void iovec_write_test(int fd, uint64_t pid, uint64_t oid)
{
	struct osd_command command;
	const char buf1[] = "If iovec_write_test works,";
	const char buf2[] = " you will see this sentence.";
	char bufout[200];
	struct bsg_iovec vec[2];
	size_t tot_len;
	int ret;

	osd_info(__func__);
	vec[0].iov_base = (uint64_t)(uintptr_t) buf1;
	vec[0].iov_len = sizeof(buf1)-1;
	vec[1].iov_base = (uint64_t)(uintptr_t) buf2;
	vec[1].iov_len = sizeof(buf2);
	tot_len = sizeof(buf1)-1 + sizeof(buf2);
	memset(&command, 0, sizeof(command));
	osd_command_set_write(&command, pid, oid, tot_len, 0);
	command.cdb_len = OSD_CDB_SIZE;
	command.outlen = tot_len;
	command.outdata = vec;
	command.iov_outlen = 2;

	ret = osd_submit_and_wait(fd, &command);
	if (ret) {
		osd_error("%s: submit_and_wait failed", __func__);
		return;
	}
	printf("%s: seemed to work\n", __func__);

	/* read it back, non-iov */
	memset(&command, 0, sizeof(command));
	memset(bufout, 0, sizeof(bufout));
	osd_command_set_read(&command, pid, oid, sizeof(bufout), 0);
	command.cdb_len = OSD_CDB_SIZE;
	command.inlen_alloc = sizeof(bufout);
	command.indata = bufout;

	ret = osd_submit_and_wait(fd, &command);
	if (ret)
		osd_error("%s: submit_and_wait failed", __func__);
	printf("%s: read some bytes (%lu): %s\n\n", __func__,
	       command.inlen, bufout);
}

static void iovec_read_test(int fd, uint64_t pid, uint64_t oid)
{
	struct osd_command command;
	const char bufout[] = "A big line of data for iovec_read_test to get.";
	char buf1[21];
	char buf2[100];
	struct bsg_iovec vec[2];
	size_t tot_len;
	int ret;

	/* write it, non-iov */
	osd_info(__func__);
	memset(&command, 0, sizeof(command));
	osd_command_set_write(&command, pid, oid, sizeof(bufout), 0);
	command.cdb_len = OSD_CDB_SIZE;
	command.outlen = sizeof(bufout);
	command.outdata = bufout;
	ret = osd_submit_and_wait(fd, &command);
	if (ret) {
		osd_error("%s: submit_and_wait failed", __func__);
		return;
	}

	memset(buf1, 0, sizeof(buf1));
	memset(buf2, 0, sizeof(buf2));
	vec[0].iov_base = (uint64_t)(uintptr_t) buf1;
	vec[0].iov_len = sizeof(buf1)-1;
	vec[1].iov_base = (uint64_t)(uintptr_t) buf2;
	vec[1].iov_len = sizeof(buf2);
	tot_len = sizeof(buf1)-1 + sizeof(buf2);
	memset(&command, 0, sizeof(command));
	osd_command_set_read(&command, pid, oid, tot_len, 0);
	command.cdb_len = OSD_CDB_SIZE;
	command.inlen_alloc = tot_len;
	command.indata = vec;
	command.iov_inlen = 2;

	ret = osd_submit_and_wait(fd, &command);
	if (ret) {
		osd_error("%s: submit_and_wait failed", __func__);
		return;
	}

	buf1[sizeof(buf1)-1] = '\0';  /* terminate partial string */
	printf("%s: read some bytes (%lu): %s + %s\n\n", __func__,
	       command.inlen, buf1, buf2);
}

static void attr_test(int fd, uint64_t pid, uint64_t oid)
{
	int i, ret;
	uint64_t len;
	uint8_t *ts;  /* odd 6-byte timestamp */
	const uint8_t data[] = "Some data.";
	/* const char attr_data[] = "An attribute.\n"; */
	struct osd_command command;

	struct attribute_list *attr, attr_proto[] = {
		{
			.type = ATTR_GET,
			.page = 0x1,  /* user info page */
			.number = 0x82,  /* logical length */
			.len = sizeof(uint64_t),
		},
		{
			.type = ATTR_GET,
			.page = 0x3,  /* user timestamp page */
			.number = 0x1,  /* ctitme */
			.len = 6,
		},
#if 0
		{
			.type = ATTR_SET,
			.page = 0x92,
			.number = 0x4,
			.val = attr_data,
			.len = sizeof(attr_data),
		},
		{
			.type = ATTR_GET_PAGE,
			.page = 0x3,  /* user timestamp page */
			.number = 0x1,  /* ctitme */
			.val = &ts,
			.len = 6,
		},
#endif
	};

	/* so length will be interesting */
	write_osd(fd, pid, oid, data, sizeof(data), 0);

	ret = osd_command_set_get_attributes(&command, pid, oid);
	if (ret) {
		osd_error("%s: set_get_attributes failed", __func__);
		printf("\n");
		return;
	}
	ret = osd_command_attr_build(&command, attr_proto,
				     ARRAY_SIZE(attr_proto));
	if (ret) {
		osd_error("%s: attr_build failed", __func__);
		printf("\n");
		return;
	}
	ret = osd_submit_and_wait(fd, &command);
	if (ret) {
		osd_error("%s: submit_and_wait failed", __func__);
		printf("\n");
		return;
	}
	ret = osd_command_attr_resolve(&command);
	if (ret) {
		osd_error("%s: attr_resolve failed", __func__);
		printf("\n");
		return;
	}
	attr = command.attr;
	for (i=0; i<ARRAY_SIZE(attr); i++) {
		if (attr[i].len != attr[i].outlen)
			osd_error("%s: attr %d short: %u not %u", __func__,
				  i, attr[i].outlen, attr[i].len);
	}
	len = get_ntohll(attr[0].val);
	ts = attr[1].val;
	printf("%s: len %016llx ts %02x%02x%02x%02x%02x%02x\n\n",
	       __func__, llu(len), ts[0], ts[1], ts[2], ts[3], ts[4], ts[5]);
	osd_command_attr_free(&command);
}

int main(int argc, char *argv[])
{
	int fd, ret, num_drives, i;
	struct osd_drive_description *drives;
	uint8_t outbuf[100];

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

		inquiry(fd);

		/*
		 * Clean the slate and make one partition.  Nothing works
		 * without a partition to put things into.
		 */
		format_osd(fd, OBJ_CAPACITY); 
		create_partition(fd, PID);


#if 0           /* These are all supposed to fail, for various reasons. */
		write_osd(fd, PID, OID, WRITEDATA, OFFSET);
		flush_osd(fd, FLUSH_SCOPE);
#endif

		
#if 0		/* Basic read / write seems to work */
		create_osd(fd, PID, OID, NUM_USER_OBJ);
		create_osd(fd, PID, OID+1, NUM_USER_OBJ);
		create_osd(fd, PID, OID+2, NUM_USER_OBJ);
		remove_osd(fd, PID, OID+1);

		write_osd(fd, PID, OID, WRITEDATA, sizeof(WRITEDATA), OFFSET);
		read_osd(fd, PID, OID, outbuf, sizeof(outbuf), OFFSET);
		append_osd(fd, PID, OID, WRITEDATA2, sizeof(WRITEDATA2));
		read_osd(fd, PID, OID, outbuf, sizeof(outbuf), OFFSET);
		write_osd(fd, PID, OID+2, WRITEDATA2, sizeof(WRITEDATA2), OFFSET);
		read_osd(fd, PID, OID+2, outbuf, sizeof(outbuf), OFFSET);
	
		read_osd(fd, PID, OID, outbuf, sizeof(outbuf), OFFSET);

#endif

#if 0           /* Testing iovec list. */
		create_osd(fd, PID, OID+3, 1);
		create_osd(fd, PID, OID+4, 1);

		iovec_write_test(fd, PID, OID+3);
		iovec_read_test(fd, PID, OID+4);

		remove_osd(fd, PID, OID+3);
		remove_osd(fd, PID, OID+4);
#endif

#if 0           /* Testing bidirectional operations. */
		create_osd(fd, PID, OID+5, 1);
		write_osd(fd, PID, OID+5, (const uint8_t *) "sixty-seven", 12, 0);
		bidi_test(fd, PID, OID+5);
		remove_osd(fd, PID, OID+5);
#endif

#if 0           /* Testing attribute API */
		create_osd(fd, PID, OID+6, 1);
		attr_test(fd, PID, OID+6);
		remove_osd(fd, PID, OID+6);
#endif


#if 1		/* Testing stuff */
		create_osd(fd, PID, OID, NUM_USER_OBJ);
		create_osd(fd, PID, OID+1, NUM_USER_OBJ);
		write_osd(fd, PID, OID, WRITEDATA, sizeof(WRITEDATA), OFFSET);
		read_osd(fd, PID, OID, outbuf, sizeof(outbuf), OFFSET);
#endif

		close(fd);
	}

	osd_free_drive_list(drives, num_drives);

	return 0;
}
