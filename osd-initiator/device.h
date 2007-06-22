#ifndef _DEVICE_H
#define _DEVICE_H

struct osd_command;

int osd_submit_command(int fd, struct osd_command *command);
int osd_wait_response(int fd, struct osd_command **command);
int osd_wait_this_response(int fd, struct osd_command *command);
int osd_submit_and_wait(int fd, struct osd_command *command);

#endif  /* _DEVICE_H */
