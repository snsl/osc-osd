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
#include "device.h"
#include "command.h"
#include "sync.h"
#include "drivelist.h"
#include "sense.h"
#include "generic_iface.h"


int gen_osd_init_drives(struct gen_osd_drive_list *drive_list)
{

	int ret, num_drives, i;

	for (i=0; i<MAX_DRIVES; i++){
		drive_list->fd_array[i] = -1;
	}

	ret = osd_get_drive_list(&(drive_list->drives), &num_drives);
	if (ret < 0) {
		osd_error("%s: get drive error", __func__);
		return 0;
	}
	if (num_drives == 0) {
		osd_error("%s: No drives found: %d", __func__, num_drives);
		return 0;
	}



	return num_drives;
}


int gen_osd_open_drive(struct gen_osd_drive_list *drive_list, int index)
{
	/*XXX Bug in osd_get_drive_list sometimes gibberish gets printed here
	need to chase this down*/
	gen_osd_debug(5, "Drive: %s, Name: %s", drive_list->drives[index].chardev,
				drive_list->drives[index].targetname);

	drive_list->fd_array[index] = open(drive_list->drives[index].chardev, O_RDWR);
	if (drive_list->fd_array[index] < 0) {
		osd_error_errno("%s: open %s", __func__,
					drive_list->drives[index].chardev);
		return -1;
	}

	return 0;
}


int gen_osd_close_drive(struct gen_osd_drive_list *drive_list, int index)
{

	if(drive_list->fd_array[index] < 0){
		osd_error("%s: Invalid Drive", __func__);
		return -1;
	}

	return close(drive_list->fd_array[index]);
}


int gen_osd_select_drive(struct gen_osd_drive_list *drive_list, int index)
{
	if(drive_list->fd_array[index] < 0){
		osd_error("%s: Drive is invalid", __func__);
		return -1;
	}

	return drive_list->fd_array[index];
}


/*after submitting command need to get result back at some point*/
int gen_osd_cmd_getcheck_res(int fd, struct osd_command *command)
{
	int ret;
	struct osd_command *cmp;

	ret = osd_wait_response(fd,&cmp);
	if (ret) {
		osd_error("%s: wait_response failed", __func__);
		return ret;
	}

	if (cmp != command) {
		osd_error("%s: wait_response returned %p, expecting %p", __func__,
		      cmp, &command);
		return -1;
	}

	return 0;
}

