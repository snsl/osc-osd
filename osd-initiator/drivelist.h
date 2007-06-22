#ifndef _DRIVELIST_H
#define _DRIVELIST_H

struct osd_drive_description {
    char *targetname;
    char *chardev;
};

int osd_get_drive_list(struct osd_drive_description **drives, int *num_drives);
void osd_free_drive_list(struct osd_drive_description *drives, int num_drives);

#endif
