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

struct gen_osd_cmd {
	struct osd_command osd_cmd;
	int current_drive;
};

struct gen_osd_drive_list {
	struct osd_drive_description *drives;
	int fd_array[MAX_DRIVES];
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

int gen_osd_init_drives(struct gen_osd_drive_list *drive_list);
int gen_osd_open_drive(struct gen_osd_drive_list *drive_list, int index);
int gen_osd_select_drive(struct gen_osd_drive_list *drive_list,
			struct gen_osd_cmd *shared, int index);
int gen_osd_close_drive(struct gen_osd_drive_list *drive_list, int index);

int cmd_set(struct gen_osd_cmd *shared, osd_cmd_val cmd, void *attrs);
int cmd_modify(void);
int cmd_submit(struct gen_osd_cmd *shared);
int cmd_get_res(struct gen_osd_cmd *shared, struct cmd_result *res);

inline void cmd_free_res(struct cmd_result *res);
void cmd_show_error(struct cmd_result *res);
#endif
