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
	struct osd_command command;
	int current_drive;
};

struct gen_osd_drive_list {
	struct osd_drive_description *drives;
	int fd_array[MAX_DRIVES];
};

/*Functions to help deal with opening devices - not used by pvfs*/
int gen_osd_init_drives(struct gen_osd_drive_list *drive_list);
int gen_osd_open_drive(struct gen_osd_drive_list *drive_list, int index);
int gen_osd_select_drive(struct gen_osd_drive_list *drive_list,
			struct gen_osd_cmd *shared, int index);
int gen_osd_close_drive(struct gen_osd_drive_list *drive_list, int index);


/*Function used to submit commands - used by test codes and pvfs*/
int gen_osd_cmd_submit(int fd, struct osd_command *cmd);


/*Functions used to build commands - used by both test codes and pvfs*/
int gen_osd_set_format(struct osd_command *cmd, uint64_t capacity);
int gen_osd_set_create_partition(struct osd_command *cmd, uint64_t pid);
int gen_osd_set_create_object(struct osd_command *cmd, uint64_t pid,
			uint64_t oid, uint16_t count);
int gen_osd_set_remove_object(struct osd_command *cmd, uint64_t pid,
			uint64_t oid);
int gen_osd_write_object(struct osd_command *cmd, uint64_t pid, uint64_t oid,
			void *buf, size_t count, size_t offset);
int gen_osd_read_object(struct osd_command *cmd, uint64_t pid, uint64_t oid,
			void *buf, size_t count, size_t offset);

/*!!! LOTS MORE OF THESE SORTS OF FUNCTIONS GO HERE*/



/*Functions to get result - test codes use first one pvfs uses second one*/
int gen_osd_cmd_getcheck_res(int fd, struct osd_command *cmd);
struct osd_command *gen_osd_cmd_get_res(int fd);


/*Functions to print error messages*/
inline void gen_osd_cmd_show_error(struct osd_command *cmd);


/*Not implemented yet*/
int gen_osd_cmd_modify(void);

#endif
