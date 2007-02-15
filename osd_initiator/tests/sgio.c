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

int main(int argc, char *argv[])
{
	int fd, ret, num_drives, i;
	struct osd_drive_description *drives;

	set_progname(argc, argv); 

	ret = osd_get_drive_list(&drives, &num_drives);
	if (ret < 0) {
		error("%s: get drive error", __func__);
		return 1;
	}
	if (num_drives == 0) {
		error("%s: no drives", __func__);
		return 1;
	}
	
	for (i=0; i<num_drives; i++) {

		printf("%s: drive %s name %s\n", progname, drives[i].chardev,
		       drives[i].targetname);
		fd = open(drives[i].chardev, O_RDWR);
		if (fd < 0) {
			error_errno("%s: open %s", __func__, drives[i].chardev);
			return 1;
		}

		inquiry_sgio(fd);
		create_osd_sgio(fd);
		write_osd_sgio(fd);
		read_osd_sgio(fd);

		close(fd);
	}

	osd_free_drive_list(drives, num_drives);

	return 0;
}
