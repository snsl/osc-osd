#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/types.h>

#include <scsi/scsi.h>
#include <scsi/sg.h>

#include "util/util.h"
#include "kernel_interface.h"
#include "cdb_manip.h"
#include "user_interface_sgio.h"
#include "diskinfo.h"
#include "sense.h"
#include "generic_iface.h"


int gen_osd_init_drives(struct gen_osd_drive_list *drive_list)
{

	int ret, num_drives, i;

	for (i=0; i<MAX_DRIVES; i++){
		drive_list->fd_array[i] = -1;
	}

	ret = osd_get_drive_list(&(drive_list->drives), &num_drives);
	if (ret < 0) {
		osd_error("%s: get drive error", __func__);
		return 0;
	}
	if (num_drives == 0) {
		osd_error("%s: No drives found: %d", __func__, num_drives);
		return 0;
	}



	return num_drives;
}


int gen_osd_open_drive(struct gen_osd_drive_list *drive_list, int index)
{
	/*XXX Bug in osd_get_drive_list sometimes gibberish gets printed here
	need to chase this down*/
	gen_osd_debug(5, "Drive: %s, Name: %s", drive_list->drives[index].chardev,
				drive_list->drives[index].targetname);

	drive_list->fd_array[index] = open(drive_list->drives[index].chardev, O_RDWR);
	if (drive_list->fd_array[index] < 0) {
		osd_error_errno("%s: open %s", __func__,
					drive_list->drives[index].chardev);
		return -1;
	}

	return 0;
}


int gen_osd_close_drive(struct gen_osd_drive_list *drive_list, int index)
{

	if(drive_list->fd_array[index] < 0){
		osd_error("%s: Invalid Drive", __func__);
		return -1;
	}

	return close(drive_list->fd_array[index]);
}


int gen_osd_select_drive(struct gen_osd_drive_list *drive_list,
			struct gen_osd_cmd *shared, int index)
{
	if(drive_list->fd_array[index] < 0){
		osd_error("%s: Drive is invalid", __func__);
		return -1;
	}

	shared->current_drive = drive_list->fd_array[index];

	return 0;
}




/*sometimes need to modify the command*/
int gen_osd_cmd_modify(void)
{

	return 0;
}

/*need to be able to submit the command*/
int gen_osd_cmd_submit(int fd, struct osd_command *command)
{

	if (fd < 0) {
		osd_error("%s: Invalid drive selected", __func__);
		return -1;
	}

	return osd_submit_command(fd, command);
}

/*after submitting command need to get result back at some point*/
int gen_osd_cmd_getcheck_res(int fd, struct osd_command *command)
{
	int ret;
	struct osd_command *cmp;

	if (fd < 0) {
		osd_error("%s: Invalid drive selected", __func__);
		return -1;
	}

	ret = osd_wait_response(fd,&cmp);
	if (ret) {
		osd_error("%s: wait_response failed", __func__);
		return ret;
	}

	if (cmp != command) {
		osd_error("%s: wait_response returned %p, expecting %p", __func__,
		      cmp, &command);
		return -1;
	}

	return 0;
}


struct osd_command *gen_osd_cmd_get_res(int fd)
{
	int ret;
	struct osd_command *out_cmd;

	ret = osd_wait_response(fd, &out_cmd);
	if(ret){
		osd_error("%s: wait_response failed", __func__);
		return NULL;
	}

	return out_cmd;
}


inline void gen_osd_cmd_show_error(struct osd_command *cmd)
{
	osd_show_sense(cmd->sense, cmd->sense_len);
	//~ osd_show_scsi_code(res->command_status);  /*implement this in sense.c since all funcs in there are static*/

}

int gen_osd_set_format(struct osd_command *cmd, uint64_t capacity)
{
	gen_osd_debug(5, "%s: Formating device %ld bytes", __func__, capacity);
	return osd_command_set_format_osd(cmd, capacity);
}

int gen_osd_set_create_partition(struct osd_command *cmd, uint64_t pid){

	gen_osd_debug(5, "%s: Create partition %ld", __func__, pid);
	return osd_command_set_create_partition(cmd, pid);
}

int gen_osd_set_create_object(struct osd_command *cmd, uint64_t pid,
			uint64_t oid, uint16_t count)
{
	if (count > 0)
		gen_osd_debug(5, "%s: Creating %d objects in partition %ld",
					__func__, count, pid);
	else
		gen_osd_debug(5, "%s: Create Object %ld.%ld",
					__func__, pid, oid);

	return osd_command_set_create(cmd, pid, oid, count);

}

int gen_osd_set_remove_object(struct osd_command *cmd, uint64_t pid,
			uint64_t oid)
{
	gen_osd_debug(5, "%s: Remove Object %ld.%ld", __func__, pid, oid);
	return osd_command_set_remove(cmd, pid, oid);

}

int gen_osd_write_object(struct osd_command *cmd, uint64_t pid, uint64_t oid,
			void *buf, size_t count, size_t offset)
{
	int ret;

	gen_osd_debug(5, "%s: Writing %ld bytes to offset %ld of %ld.%ld",
			 __func__, count, offset, pid, oid );

	ret = osd_command_set_write(cmd, pid, oid, count, offset);
	cmd->outdata = buf;
	cmd->outlen = count;
	return ret;
}

int gen_osd_read_object(struct osd_command *cmd, uint64_t pid, uint64_t oid,
			void *buf, size_t count, size_t offset)
{
	int ret;

	gen_osd_debug(5, "%s: Reading %ld bytes at offset %ld of %ld.%ld",
			 __func__, count, offset, pid, oid );

	ret = osd_command_set_read(cmd, pid, oid, count, offset);
	cmd->outdata = buf;
	cmd->outlen = count;
	return ret;
}
