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
#ifndef _DRIVELIST_H
#define _DRIVELIST_H

struct osd_drive_description {
    char *targetname;
    char *chardev;
};

int osd_get_drive_list(struct osd_drive_description **drives, int *num_drives);
void osd_free_drive_list(struct osd_drive_description *drives, int num_drives);

#endif
