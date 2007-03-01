/*
 * My dream test program.  Exercise the command interface.
 */
#include "initiator.h"

/*
 * Handy wrapper just for this test program.  Real usage will
 * submit multiple commands asynchronously and handle results
 * through a separate state machine, possibly out of order.
 */
static void run_command(int fd, struct osd_command *command, const char *name)
{
	int ret;
	struct command *returned_command;

	ret = osd_submit_command(fd, command);
	if (ret)
		osd_error_xerrno(ret);
	ret = osd_wait_response(fd, &returned_command);
	if (ret)
		osd_error_xerrno(ret);
	if (command != returned_command)
		osd_error();
	if (command.status != 0)
		osd_sense_show(command);
}

static void run_tests(int fd)
{
	struct osd_command command;

	/*
	 * Basic inquiry.
	 */
	uint8_t inquiry_response[80];
	osd_command_inquiry(&command, inquiry_response,
	                    sizeof(inquiry_response));
	run_command(fd, &command, "inquiry");
	printf("inquiry results\n");
	osd_hexdump(inquiry_response, command.inlen);


	/*
	 * Clean the slate and make one partition.
	 */
	osd_command_format_osd(&command, 1 << 30);
	run_command(fd, &command, "format");
	printf("formatted\n");

	osd_command_create_partition(&command, PID);
	run_command(fd, &command, "create partition");
	printf("created partition 0x%llx\n", PID);


	/*
	 * These are all supposed to fail, for various reasons.
	 */
	osd_command_write(PID, OID, WRITEDATA, LENGTH, OFFSET);
	run_command(fd, &command, "write (should fail, no pid)");

	osd_command_write(PID, OID, WRITEDATA, LENGTH, OFFSET);
	run_command(fd, &command, "flush (should fail, unimplemented)");

	
	/*
	 * Basic create / remove / read / write should work.
	 */
	uint64_t suggested_oid = OID;
	osd_command_create(&command, PID, suggested_oid, 1);
	run_command(fd, &command, "create specific");
	printf("created object id 0x%llx\n", llu(suggested_oid));

	osd_command_remove(&command, PID, suggested_oid);
	run_command(fd, &command, "remove");
	printf("removed object id 0x%llx\n", llu(suggested_oid));

	uint64_t oid;
	uint8_t attr_tmp_buf[1024];
	struct attribute_id get_oid_attr = {
		.op = GET_ATTRIBUTE,
		.page = ATTR_PAGE_CURRENT_COMMAND,
		.number = ATTR_NUMBER_CURRENT_COMMAND_OID,
		.buf = &oid,
		.len = sizeof(oid),
	}
	osd_command_create(&command, PID, 0, 3);
	osd_command_modify_attrs(&command, &get_oid_attr, 1, attr_tmp_buf);
	run_command(fd, &command, "create 3 anonymous");
	printf("created 3 object ids from 0x%llx\n", llu(oid));

	osd_command_remove(&command, PID, oid+1);
	run_command(fd, &command, "remove");
	printf("removed object id 0x%llx\n", llu(oid+1));

	const char writebuf[] = "Hello world.";
	uint8_t tmp_buf[1024];
	osd_command_write(&command, PID, oid, writebuf, sizeof(writebuf), 0);
	osd_command_modify_attrs(&command, &get_oid_attr, 1, attr_tmp_buf);

	...etc...

	write_osd_sgio(fd, PID, OID, WRITEDATA, OFFSET);
	read_osd_sgio(fd, PID, OID, OFFSET);
	write_osd_sgio(fd, PID, OID+2, WRITEDATA2, OFFSET);
	read_osd_sgio(fd, PID, OID+2, OFFSET);

	read_osd_sgio(fd, PID, OID, OFFSET);

#endif

#if 1           /* Testing bidirectional operations. */
	bidi_test(fd);
#endif


#if 0		/* Testing stuff */
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
			osd_error_errno("%s: open %s", __func__,
			                drives[i].chardev);
			return 1;
		}

		run_tests(fd);
	}

	osd_free_drive_list(drives, num_drives);
	return 0;
}

