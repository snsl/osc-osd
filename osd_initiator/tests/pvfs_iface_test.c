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

#include "pvfs_iface.h"

#define PVFS_OSD_PID 0x10003ULL
static const int OBJ_CAPACITY = 1<<30; /* 1 GB */

int main(void)
{
	int ret;
	struct partition_attrs partition;
	struct format_attrs format;
	struct pvfs_osd opaque_handle;
	struct cmd_result res;
	int drive_id;

	printf("Initialization\n");
	ret = pvfs_osd_init_drives(&opaque_handle);
	if (ret == 0){
		printf("Can't init drives\n");
		return -1;
	}

	printf("Found %d Drives Now open a drive\n", ret);
	drive_id = 0;  /*First drive*/
	ret = pvfs_osd_open_drive(&opaque_handle, drive_id);
	if (ret != 0){
		printf("Unable to open drive\n");
		return -1;
	}

	printf("Drive opened making active\n");
	ret = pvfs_osd_select_drive(&opaque_handle, drive_id);
	if (ret != 0){
		printf("Unable to select drive\n");
		return -1;
	}

	printf("Formatting active disk\n");
	format.capacity = OBJ_CAPACITY;
	printf(".....Set format command\n");
	ret = cmd_set(&opaque_handle, FORMAT, &format);
	if (ret != 0){
		printf("Couldn't set format command\n");
		return -1;
	}
	printf(".....Submit the command\n");
	ret = cmd_submit(&opaque_handle);
	if (ret != 0){
		printf("Submit command failed\n");
		return -1;
	}
	printf(".....Get the status of the command\n");
	ret = cmd_get_res(&opaque_handle, &res);
	if (ret != 0) {
		printf("Unable to get result\n");
		return -1;
	}

	if(res.sense_len != 0){
		printf("Sense data found!\n");
		return -1;
	}

	if(res.command_status != 0){
		printf("Command FAILED!\n");
		return -1;
	}
	printf(".....Response len is %d\n", (int) res.resp_len);
	printf(".....%s\n", (char *) res.resp_data);





	//~ partition.pid = PVFS_OSD_PID;

	printf("Close the drive\n");
	ret = pvfs_osd_close_drive(&opaque_handle, drive_id);
	if (ret != 0){
		printf("Unable to close drive\n");
		return -1;
	}




	return 0;
}
