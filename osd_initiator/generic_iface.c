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
#include "user_interface_sgio.h"
#include "diskinfo.h"
#include "cdb_manip.h"
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

/*sets the command up in the CDB*/
int cmd_set(struct gen_osd_cmd *shared, osd_cmd_val cmd, void *attrs)
{
	/*Trusting codes not to pass in something nasty in attrs*/
	struct partition_attrs *part;
	struct format_attrs *format;
	struct object_attrs *object;
	struct io_attrs *write, *read;

	memset(&(shared->osd_cmd), 0, sizeof(shared->osd_cmd));
	shared->osd_cmd.cdb_len = OSD_CDB_SIZE;

	switch (cmd) {
	case CREATE_PART:
		part = attrs;
		gen_osd_debug(5, "%s: Create partition %ld", __func__, part->pid);
		return set_cdb_osd_create_partition(shared->osd_cmd.cdb, part->pid);
	case FORMAT:
		format = attrs;
		gen_osd_debug(5, "%s: Format", __func__);
		return set_cdb_osd_format_osd(shared->osd_cmd.cdb, format->capacity);
	case CREATE_OBJECT:
		object = attrs;
		/*TODO: Need to take in args for setting attributes*/
		if(object->count > 0)
			gen_osd_debug(5, "%s: Creating %d objects in partition %ld",
					__func__, object->count, object->pid);
		else
			gen_osd_debug(5, "%s: Create Object %ld.%ld",
					__func__, object->pid, object->oid);
		return set_cdb_osd_create(shared->osd_cmd.cdb, object->pid,
					object->oid, object->count);
	case REMOVE_OBJECT:
		object = attrs;
		gen_osd_debug(5, "%s: Remove Object %ld.%ld",
					__func__, object->pid, object->oid);
		return set_cdb_osd_remove(shared->osd_cmd.cdb, object->pid,
				object->oid);
	case WRITE:
		write = attrs;
		gen_osd_debug(5, "%s: Writing %ld bytes to offset %ld of %ld.%ld",
				__func__, write->len, write->offset, write->pid,
				write->oid );
		shared->osd_cmd.outdata = write->write_buf;
		shared->osd_cmd.outlen	= write->len;
		return set_cdb_osd_write(shared->osd_cmd.cdb, write->pid,
					write->oid, write->len, write->offset);
	case READ:
		read = attrs;
		gen_osd_debug(5, "%s: Reading %ld bytes at offset %ld of %ld.%ld",
				__func__, read->len, read->offset, read->pid,
				read->oid );
		shared->osd_cmd.outdata = read->read_buf;
		shared->osd_cmd.outlen	= read->len;
		return set_cdb_osd_read(shared->osd_cmd.cdb, read->pid,
					read->oid, read->len, read->offset);


	default:
		return -EINVAL;
	}

}


/*sometimes need to modify the command*/
int cmd_modify(void)
{

	return 0;
}

/*need to be able to submit the command*/
int cmd_submit(struct gen_osd_cmd *shared)
{


	if(shared->current_drive < 0){
		osd_error("%s: Invalid drive selected", __func__);
		return -1;
	}

	/*func name is not really apropriate buy why duplicate code*/
	return osd_sgio_submit_command(shared->current_drive, &(shared->osd_cmd));
}

/*after submitting command need to get result back at some point*/
int cmd_get_res(struct gen_osd_cmd *shared, struct cmd_result *res)
{
	int ret;
	struct osd_command *cmp;

	if(shared->current_drive < 0){
		osd_error("%s: Invalid drive selected", __func__);
		return -1;
	}

	/*not duplicating code*/
	ret = osd_sgio_wait_response(shared->current_drive,&cmp);
	if (ret) {
		osd_error("%s: wait_response failed", __func__);
		return ret;
	}
	if (cmp != &(shared->osd_cmd)) {
		osd_error("%s: wait_response returned %p, expecting %p", __func__,
		      cmp, &(shared->osd_cmd));
		return -1;
	}

	res->sense_len = cmp->sense_len;
	res->command_status = cmp->status;
	res->resp_len = cmp->inlen;

	/*copy the sense data because its small but just keep pointer to
	actual returned data, idea is to be able to post multiple osd commands
	then get the results back so need to remember where the data is because
	in the set_cmd fun we obliterate anything in there, since could
	be fairly big don't want to memcpy*/
	memcpy(res->sense_data, cmp->sense, OSD_MAX_SENSE);
	res->resp_data = cmp->indata;

	return 0;
}

inline void cmd_free_res(struct cmd_result *res)
{
	free(res->resp_data);  /*free is a void fun so nothing to return*/
}

inline void cmd_show_error(struct cmd_result *res)
{
	osd_show_sense(res->sense_data, res->sense_len);
	//~ osd_show_scsi_code(res->command_status);  /*implement this in sense.c since all funcs in there are static*/

}
