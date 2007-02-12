#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>

#include "util/util.h"

#include "interface.h"
#include "diskinfo.h"
#include "kernel/suo_ioctl.h"


char *osd_get_drive_serial(int fd)
{
	struct osd_command command;
	char buf[512];
	int ret;

	memset(buf, 0, sizeof(buf));
	memset(&command, 0, sizeof(command));

	/* See http://en.wikipedia.org/wiki/SCSI_Inquiry_Command */
	command.cdb[0] = 0x12; 	/* SCSI INQUIRY */
	command.cdb[1] = 1; 	/* Enable extra info */
	command.cdb[2] = 0x80;   /* Serial number */
	command.cdb[4] = 252;	/* Length */

	command.cdb_len = 6;
	command.indata = buf;
	command.inlen_alloc = 512;

	ret = osd_sgio_submit_and_wait(fd, &command);
	if (ret) {
		printf("Submit failed!\n");
		return NULL;
	}

	return strdup(buf+4);
}

int osd_get_drive_list(struct osd_drive_description **drives, int *num_drives)
{
	struct osd_drive_description *ret = NULL;
	struct dirent *entry;
	int count = 0, fd;
	char buf[512];
	char *chardev, *serial;

	/* Walk along sysfs looking for the drives */
	DIR *toplevel = opendir("/sys/class/scsi_generic");
	if (!toplevel)
		goto out;

	/* First, get the count */
	while ((entry = readdir(toplevel))) {
		count++;
	}

	if (count == 0)
		goto out;

	ret = malloc(count * sizeof(*ret));
	memset(ret, 0, count * sizeof(*ret));
	rewinddir(toplevel);
	count = 0;
	while ((entry = readdir(toplevel))) {
		int type;

		snprintf(buf, sizeof(buf), "/sys/class/scsi_generic/%s/device/type",
		         entry->d_name);
		fd = open(buf, O_RDONLY);
		if (fd < 0)
			continue;

		buf[0] = '\0';
		read(fd, buf, sizeof(buf));
		sscanf(buf, "%d", &type);
		close(fd);

		if (type != 17)  /* TYPE_OSD */
			continue;

		sprintf(buf, "/dev/%s", entry->d_name);

		fd = open(buf, O_RDWR);
		if (fd <= 0)
			continue;

		serial = osd_get_drive_serial(fd);
		close(fd);

		if (!serial)
			continue;

		ret[count].targetname = serial;
		ret[count].chardev = strdup(buf);
		count++;
	}

out:
	if (toplevel)
		closedir(toplevel);
	*drives = ret;
	*num_drives = count;
	return 0;
}

void osd_free_drive_list(struct osd_drive_description *drives, int num_drives)
{
	struct osd_drive_description *iter;
	int i;

	for (i=0; i<num_drives; i++) {
		free(drives[i].targetname);
		free(drives[i].chardev);
	}
	free(drives);
}

