#ifndef generic_iface_hdr
#define generic_iface_hdr

#define MAX_DRIVES 100

#if 1
#define gen_osd_iface_debug 10
#define gen_osd_debug(lvl,fmt,args...) \
    do { \
	if (lvl <= gen_osd_iface_debug) \
	    printf("DEBUG: " fmt "\n", ##args);\
    } while (0)
#else
#  define gen_osd_debug(lvl,fmt,...) do { } while (0)
#endif

struct gen_osd_drive_list {
	struct osd_drive_description *drives;
	int fd_array[MAX_DRIVES];
};

int gen_osd_init_drives(struct gen_osd_drive_list *drive_list);
int gen_osd_open_drive(struct gen_osd_drive_list *drive_list, int index);
int gen_osd_select_drive(struct gen_osd_drive_list *drive_list, int index);
int gen_osd_close_drive(struct gen_osd_drive_list *drive_list, int index);

int gen_osd_cmd_getcheck_res(int fd, struct osd_command *cmd);

#endif
