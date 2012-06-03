/*
 * Test alignment issues with submitting userspace buffers through bsg.
 *
 * Copyright (C) 2008 Pete Wyckoff <pw@osc.edu>
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
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

#include "osd-util/osd-util.h"
#include "command.h"
#include "device.h"
#include "drivelist.h"

int main(int argc, char *argv[])
{
	int fd, ret, num_drives, i, shift;
	struct osd_drive_description *drives;
	struct osd_command command;
	void *buf;

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
	
	ret = posix_memalign(&buf, 4096, 4096);
	if (ret < 0) {
		osd_error_errno("%s: posix_memasign", __func__);
		return 1;
	}

	for (i=0; i<num_drives; i++) {

		printf("%s: drive %s name %s\n", osd_get_progname(),
		       drives[i].chardev, drives[i].targetname);
		fd = open(drives[i].chardev, O_RDWR);
		if (fd < 0) {
			osd_error_errno("%s: open %s", __func__,
					drives[i].chardev);
			return 1;
		}

		for (shift = 10; shift >= 0; shift--) {

			osd_command_set_inquiry(&command, 0, 80);
			command.indata = (uint8_t *) buf + (1 << shift);
			command.inlen_alloc = 512;

			printf("buf %lx len %zu\n", (uintptr_t) command.indata,
			       command.inlen_alloc);
			ret = osd_submit_command(fd,  &command);
			if (ret < 0) {
				osd_error_errno("%s: osd_submit_command failed",
						__func__);
				continue;
			}

			ret = osd_wait_this_response(fd, &command);
			if (ret < 0) {
				osd_error_errno(
					"%s: osd_wait_this_response failed",
					__func__);
				continue;
			}

			if (command.status != 0) {
				osd_error("%s: status %d", __func__,
					  command.status);
				continue;
			}
		}

		close(fd);
	}

	osd_free_drive_list(drives, num_drives);

	return 0;
}
