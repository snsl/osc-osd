#ifndef pvfs_iface_hdr
#define pvfs_iface_hdr


#define MAX_DRIVES 100


#if 1
#define pvs_osd_iface_debug 10
#define pvfs_osd_debug(lvl,fmt,args...) \
    do { \
	if (lvl <= pvs_osd_iface_debug) \
	    printf("DEBUG: " fmt "\n", ##args);\
    } while (0)
#else
#  define pvfs_osd_debug(lvl,fmt,...) do { } while (0)
#endif

struct pvfs_osd {
	struct osd_command osd_cmd;
	struct osd_drive_description *drives;
	int fd_array[MAX_DRIVES];
	int current_drive;
};

struct partition_attrs {
	uint64_t pid;
};

struct format_attrs {
	uint32_t capacity;
};

struct cmd_result {
	uint32_t 	sense_len;
	uint8_t 	command_status;
	size_t 		resp_len;
	uint8_t 	sense_data[OSD_MAX_SENSE];
	void 		*resp_data;
};

typedef enum{
	CREATE_PART = 1,
	FORMAT,

}osd_cmd_val;

int pvfs_osd_init_drives(struct pvfs_osd *shared);
int pvfs_osd_open_drive(struct pvfs_osd *shared, int index);
int pvfs_osd_select_drive(struct pvfs_osd *shared, int index);
int pvfs_osd_close_drive(struct pvfs_osd *shared, int index);

int cmd_set(struct pvfs_osd *shared, osd_cmd_val cmd, void *attrs);
int cmd_modify(void);
int cmd_submit(struct pvfs_osd *shared);
int cmd_get_res(struct pvfs_osd *shared, struct cmd_result *res);

inline void cmd_free_res(struct cmd_result *res);
void cmd_show_error(struct cmd_result *res);
#endif
