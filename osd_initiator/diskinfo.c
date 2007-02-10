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
#include "osd_hdr.h"
#include "../kernel/suo_ioctl.h"


char *osd_get_drive_serial(int fd)
{
	struct osd_command command;
	char buf[512];
	int ret;
	unsigned char cdb[] = {0,0,0,0,0,0};

	bzero(buf, 512);

	/* See http://en.wikipedia.org/wiki/SCSI_Inquiry_Command */
	cdb[0] = 0x12; 	/* SCSI INQUIRY */
	cdb[1] = 1; 	/* Enable extra info */
	cdb[2] = 0x80;   /* Serial number */
	cdb[4] = 6;	/* Length */

	memset(&command, 0, sizeof(command));
	command.cdb = cdb;
	command.cdb_len = 6;
	command.indata = buf;
	command.inlen_alloc = 512;

	ret = osd_submit_command(fd, &command, DMA_NONE);
	if (ret < 0) {
		printf("Read failed!\n");
		printf("buf = '%s'\n", buf);
		return NULL;
	}

	ret = dev_osd_wait_response(fd, NULL);
	if (ret < 0) {
		printf("Wait response failed!\n");
		printf("buf = '%s'\n", buf);
		return NULL;
	}

	return strdup(buf);
}

static char *get_chardev_from_sysfs(const char *root)
{
	char *ret = NULL;
	struct dirent *entry;
	char buf[512];

	DIR *toplevel = opendir(root);
	if (!toplevel)
		return strdup(ret);

	while ((entry = readdir(toplevel))) {
		if (entry->d_name[0] == 's' &&
		   entry->d_name[1] == 'u') {
			snprintf(buf, sizeof(buf), "/dev/%s", entry->d_name);
			ret = strdup(buf);
			break;
		}
	}
	closedir(toplevel);

	return ret;
}

struct osd_drive_description *osd_get_drive_list(void)
{
	struct osd_drive_description *ret = NULL;
	struct dirent *entry;
	int count = 0, fd;
	char buf[512];
	char *chardev, *serial;

	/* Walk along sysfs looking for the drives */
	DIR *toplevel = opendir("/sys/class/scsi_osd");
	if (!toplevel)
		goto out;

	/* First, get the count */
	while ((entry = readdir(toplevel))) {
		count++;
	}

	if (count == 0)
		goto out;

	ret = malloc( sizeof(*ret) * (count+1));
	memset(ret, 0, sizeof(*ret) * (count+1));
	rewinddir(toplevel);
	count = 0;
	while ((entry = readdir(toplevel))) {
		snprintf(buf, sizeof(buf), "/sys/class/scsi_osd/%s",
		         entry->d_name);
		chardev = get_chardev_from_sysfs(buf);
		if (!chardev)
			continue;

		fd = dev_osd_open(chardev);
		if (fd <= 0) {
			free(chardev);
			continue;
		}

		serial = osd_get_drive_serial(fd);
		dev_osd_close(fd);

		if (!serial) {
			free(chardev);
			continue;
		}

		ret[count].targetname = serial;
		ret[count].chardev = chardev;
		count++;
	}

out:
	if (toplevel)
		closedir(toplevel);
	return ret;
}

void osd_drive_list_free(struct osd_drive_description *drive_list)
{
	struct osd_drive_description *iter;

	for (iter = drive_list; iter != NULL; iter++) {
		free(iter->targetname);
		free(iter->chardev);
	}
	free(drive_list);
}

