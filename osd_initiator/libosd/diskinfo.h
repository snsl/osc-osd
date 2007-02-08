#ifndef _DISKINFO_H
#define _DISKINFO_H

struct osd_drive_description {
    char *targetname;
    char *chardev;
};

char *osd_get_drive_serial(int fd);
struct osd_drive_description *osd_get_drive_list(void);
void osd_drive_list_free(struct osd_drive_description *drive_list);

#endif
