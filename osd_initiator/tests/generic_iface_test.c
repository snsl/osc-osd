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

#include "generic_iface.h"

#define PVFS_OSD_PID 0x10003ULL
static const int OBJ_CAPACITY = 1<<30; /* 1 GB */

struct gen_osd opaque_handle;
int drive_id;


void init(void);
void format(void);
void fini(void);
void create_partition(void);

void init(void)
{
	int ret;
	printf("Initialization\n");
	ret = gen_osd_init_drives(&opaque_handle);
	if (ret == 0){
		printf("Can't init drives\n");
		exit(1);
	}

	printf("Found %d Drives Now open a drive\n", ret);
	drive_id = 0;  /*First drive*/
	ret = gen_osd_open_drive(&opaque_handle, drive_id);
	if (ret != 0){
		printf("Unable to open drive\n");
		exit(1);
	}

	printf("Drive opened making active\n");
	ret = gen_osd_select_drive(&opaque_handle, drive_id);
	if (ret != 0){
		printf("Unable to select drive\n");
		exit(1);
	}

}

void format(void)
{
	struct format_attrs format;
	int ret;
	struct cmd_result res;

	printf("Formatting active disk\n");
	format.capacity = OBJ_CAPACITY;
	printf(".....Set format command\n");
	ret = cmd_set(&opaque_handle, FORMAT, &format);
	if (ret != 0){
		printf("Couldn't set format command\n");
		exit(1);
	}
	printf(".....Submit the command\n");
	ret = cmd_submit(&opaque_handle);
	if (ret != 0){
		printf("Submit command failed\n");
		exit(1);
	}
	printf(".....Get the status of the command\n");
	ret = cmd_get_res(&opaque_handle, &res);
	if (ret != 0) {
		printf("Unable to get result\n");
		exit(1);
	}

	if(res.sense_len != 0){
		printf("Sense data found!\n");
		exit(1);
	}

	if(res.command_status != 0){
		printf("Command FAILED!\n");
		exit(1);
	}
	printf(".....Response len is %d\n", (int) res.resp_len);
	printf(".....%s\n", (char *) res.resp_data);

	cmd_free_res(&res);

}

void fini(void)
{
	int ret;
	printf("Close the drive\n");
	ret = gen_osd_close_drive(&opaque_handle, drive_id);
	if (ret != 0){
		printf("Unable to close drive\n");
		exit(1);
	}
}

void create_partition(void)
{
	int ret;
	struct partition_attrs partition;
	struct cmd_result res;
	struct cmd_result res2;

	printf("Partitioning active disk - SHOULD FAIL\n");
	partition.pid = 2;

	printf(".....Set format command\n");
	ret = cmd_set(&opaque_handle, CREATE_PART, &partition);
	if (ret != 0){
		printf("Couldn't set create partition command\n");
		exit(1);
	}

	printf(".....Submit the command\n");
	ret = cmd_submit(&opaque_handle);
	if (ret != 0){
		printf("Submit command failed\n");
		exit(1);
	}

	printf("Partitioning active disk\n");
	partition.pid = PVFS_OSD_PID;

	printf(".....Set format command\n");
	ret = cmd_set(&opaque_handle, CREATE_PART, &partition);
	if (ret != 0){
		printf("Couldn't set create partition command\n");
		exit(1);
	}

	printf(".....Submit the command\n");
	ret = cmd_submit(&opaque_handle);
	if (ret != 0){
		printf("Submit command failed\n");
		exit(1);
	}

	printf(".....Get the status of the command\n");
	ret = cmd_get_res(&opaque_handle, &res);
	if (ret != 0) {
		printf("Unable to get result\n");
		exit(1);
	}

	if(res.sense_len == 0){
		printf("No Sense data found when there should have been!\n");
		exit(1);
	}


	if(res.command_status == 0){
		printf("Command did not FAIL when it should!\n");
		exit(1);
	}
	printf(".....Response len is %d\n", (int) res.resp_len);
	printf(".....%s\n", (char *) res.resp_data);

	printf(".....Command failed as expected\n");



	printf(".....Get the status of the command\n");
	ret = cmd_get_res(&opaque_handle, &res2);
	if (ret != 0) {
		printf("Unable to get result\n");
		exit(1);
	}

	if((res2.sense_len != 0) || (res2.command_status != 0)){
		printf("Sense data found means error!\n");
		cmd_show_error(&res2);
		exit(1);
	}

	printf(".....Response len is %d\n", (int) res2.resp_len);
	printf(".....%s\n", (char *) res2.resp_data);

	printf(".....Command worked successfully\n");
	cmd_free_res(&res2);

	printf("Problem with the first command was:\n");
	cmd_show_error(&res);


	cmd_free_res(&res);

}
int main(void)
{

	init();

	format();

	create_partition();

	fini();


	return 0;
}
