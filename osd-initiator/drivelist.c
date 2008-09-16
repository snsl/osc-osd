/*
 * Drive list.
 *
 * Copyright (C) 2007 OSD Team <pvfs-osd@osc.edu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>

#include "osd-util/osd-util.h"
#include "command.h"
#include "device.h"
#include "drivelist.h"

/*
 * Ask for page 0x80 to get serial number.  Returns a new string if it
 * succeeds.
 */
static char *osd_get_drive_serial(int fd)
{
	struct osd_command command;
	char *s, *p, buf[80];
	int ret;
	unsigned int len;

	memset(buf, 0, sizeof(buf));
	osd_command_set_inquiry(&command, 0x80, sizeof(buf));
	command.cdb[2] = 0x80;  
	command.indata = buf;
	command.inlen_alloc = sizeof(buf);

	ret = osd_submit_and_wait(fd, &command);
	if (ret) {
		osd_error_xerrno(ret, "%s: osd_submit_and_wait failed",
				 __func__);
		return NULL;
	}
	if (command.inlen < 4)
		return NULL;
	len = buf[3];
	if (len == 0)
		return NULL;
	if (len < command.inlen - 4)
		len = command.inlen - 4;
	/* trim spaces left and right */
	p = &buf[4];
	while (len > 0 && *p == ' ') {
		++p;
		--len;
	}
	if (len == 0)
		return NULL;
	while (len > 0 && p[len-1] == ' ')
		--len;
	if (len == 0)
		return NULL;
	s = malloc(len + 1);
	if (!s)
		return NULL;
	memcpy(s, p, len);
	s[len] = '\0';

	return s;
}

int osd_get_drive_list(struct osd_drive_description **drives, int *num_drives)
{
	struct osd_drive_description *ret = NULL;
	struct dirent *entry;
	int count, fd, type;
	char buf[512];
	char *serial;
	DIR *toplevel;

	/*
	 * Walk through /dev/bsg to find available devices.  Could look
	 * through /sys/class/bsg, but would have to create each device
	 * by hand in /tmp somewhere to use it, or figure out the mapping
	 * in /dev anyway.
	 */
	count = 0;
	toplevel = opendir("/dev/bsg");
	if (!toplevel)
		goto out;

	/* First, get the count */
	while ((entry = readdir(toplevel)))
		++count;

	/* subtract 2 for . and .. */
	count -= 2;

	if (count <= 0)
		goto out;

	ret = malloc(count * sizeof(*ret));
	memset(ret, 0, count * sizeof(*ret));
	rewinddir(toplevel);
	count = 0;
	while ((entry = readdir(toplevel))) {
		snprintf(buf, sizeof(buf),
		         "/sys/class/scsi_device/%s/device/type",
		         entry->d_name);
		fd = open(buf, O_RDONLY);
		if (fd < 0)
			continue;

		type = 0;
		buf[0] = '\0';
		read(fd, buf, sizeof(buf));
		sscanf(buf, "%d", &type);
		close(fd);

		if (type != 17)  /* TYPE_OSD */
			continue;

		snprintf(buf, sizeof(buf), "/dev/bsg/%s", entry->d_name);

		fd = open(buf, O_RDWR);
		if (fd < 0)
			continue;

		serial = osd_get_drive_serial(fd);
		close(fd);

		if (!serial)
			continue;

		ret[count].targetname = serial;
		ret[count].chardev = strdup(buf);
		++count;
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
	int i;

	for (i=0; i<num_drives; i++) {
		free(drives[i].targetname);
		free(drives[i].chardev);
	}
	free(drives);
}

