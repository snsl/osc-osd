#ifndef _DISKINFO_H
#define _DISKINFO_H

char *osd_get_drive_serial(int fd);
char **osd_get_drive_list(void);
void osd_drive_list_free(char **drive_list);

#endif
