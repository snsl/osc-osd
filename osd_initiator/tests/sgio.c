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
static const uint16_t NUM_USER_OBJ = 1;
static const uint64_t OFFSET = 0;
static const int FLUSH_SCOPE = 2; /* Flush everything */
static const int OBJ_CAPACITY = 1<<30; /* 1 GB */
static const char WRITEDATA[] = "Write some data";
static const char WRITEDATA2[] = "Test #2";

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
		flush_osd_sgio(fd, FLUSH_SCOPE);
		format_osd_sgio(fd, OBJ_CAPACITY); 
		sleep(2);

		
#if 1		/* Basic read / write seems to work */
		create_osd_sgio(fd, PID, OID, NUM_USER_OBJ+2);
		remove_osd_sgio(fd, PID, OID+1);

		write_osd_sgio(fd, PID, OID, WRITEDATA, OFFSET);
		read_osd_sgio(fd, PID, OID, OFFSET);
		write_osd_sgio(fd, PID, OID+2, WRITEDATA2, OFFSET);
		read_osd_sgio(fd, PID, OID+2, OFFSET);
	
		read_osd_sgio(fd, PID, OID, OFFSET);

#endif
#if 1		/* Testing stuff */

#endif
		close(fd);
	}

	osd_free_drive_list(drives, num_drives);

	return 0;
}
