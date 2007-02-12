#ifndef _DISKINFO_H
#define _DISKINFO_H

struct osd_drive_description {
    char *targetname;
    char *chardev;
};

char *osd_get_drive_serial(int fd);
int osd_get_drive_list(struct osd_drive_description **drives, int *num_drives);
void osd_free_drive_list(struct osd_drive_description *drives, int num_drives);

#endif
