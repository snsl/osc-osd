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

#include "generic_iface.h"

#define PVFS_OSD_PID 0x10003ULL
#define FIRST_OID  0x10000LLU;

static const int OBJ_CAPACITY = 1<<30; /* 1 GB */


int drive_id;
struct gen_osd_drive_list drive_list;
int osd_fd;
char *buf;
char *buf2;

void init(void);
void format(void);
void fini(void);
void create_partition(void);
void create_objects(void);
void remove_objects(void);
void write_objects(void);
void read_objects(void);


void init(void)
{
	int ret;

	printf("Initialization\n");
	ret = gen_osd_init_drives(&drive_list);
	if (ret == 0){
		printf("Can't init drives\n");
		exit(1);
	}

	printf("Found %d Drives Now open a drive\n", ret);
	drive_id = 0;  /*First drive*/

	ret = gen_osd_open_drive(&drive_list, drive_id);
	if (ret != 0){
		printf("Unable to open drive\n");
		exit(1);
	}



}

void format(void)
{
	struct format_attrs format;
	int ret;
	struct cmd_result res;
	struct gen_osd_cmd cmd;


	printf("Setting FD (could set cmd.current_drive directly ie PVFS)\n");
	ret = gen_osd_select_drive(&drive_list, &cmd, drive_id);
	if (ret != 0){
		printf("Unable to select drive\n");
		exit(1);
	}

	printf("Formatting active disk\n");
	format.capacity = OBJ_CAPACITY;
	printf(".....Set format command\n");
	ret = cmd_set(&cmd, FORMAT, &format);
	if (ret != 0){
		printf("Couldn't set format command\n");
		exit(1);
	}
	printf(".....Submit the command\n");
	ret = cmd_submit(&cmd);
	if (ret != 0){
		printf("Submit command failed\n");
		exit(1);
	}
	printf(".....Get the status of the command\n");
	ret = cmd_get_res(&cmd, &res);
	if (ret != 0) {
		printf("Unable to get result\n");
		exit(1);
	}

	if(res.sense_len != 0){
		printf("Sense data found!\n");
		exit(1);
	}

	if(res.command_status != 0){
		printf("Command FAILED!\n");
		exit(1);
	}
	printf(".....Response len is %d\n", (int) res.resp_len);
	printf(".....%s\n", (char *) res.resp_data);

	cmd_free_res(&res);

}

void fini(void)
{
	int ret;
	printf("Close the drive\n");
	ret = gen_osd_close_drive(&drive_list, drive_id);
	if (ret != 0){
		printf("Unable to close drive\n");
		exit(1);
	}
}

void create_partition(void)
{
	int ret;
	struct partition_attrs partition;
	struct cmd_result res;
	struct cmd_result res2;
	struct gen_osd_cmd cmd;
	struct gen_osd_cmd cmd2;

	printf("Setting FD for cmd non-PVFS way\n");
	ret = gen_osd_select_drive(&drive_list, &cmd, drive_id);
	if (ret != 0){
		printf("Unable to select drive\n");
		exit(1);
	}

	osd_fd = cmd.current_drive;
	printf("Setting FD for cmd 2 in PVFS way\n");
	cmd2.current_drive = osd_fd;


	partition.pid = 2;
	printf("Create Partition command that should fail\n");
	ret = cmd_set(&cmd, CREATE_PART, &partition);
	if (ret != 0){
		printf("Couldn't set create partition command\n");
		exit(1);
	}

	partition.pid = PVFS_OSD_PID;
	printf("Create Partition command that should work\n");
	ret = cmd_set(&cmd2, CREATE_PART, &partition);
	if (ret != 0){
		printf("Couldn't set create partition command\n");
		exit(1);
	}

	printf("Submit the commands\n");
	ret = cmd_submit(&cmd);
	if (ret != 0){
		printf("Submit command failed\n");
		exit(1);
	}
	ret = cmd_submit(&cmd2);
	if (ret != 0){
		printf("Submit command failed\n");
		exit(1);
	}

	printf("Get the status of the commands\n\n");

	ret = cmd_get_res(&cmd, &res);
	if (ret != 0) {
		printf("Unable to get result\n");
		exit(1);
	}
	if(res.sense_len == 0){
		printf("No Sense data found when there should have been!\n");
		exit(1);
	}
	if(res.command_status == 0){
		printf("Command did not FAIL when it should!\n");
		exit(1);
	}
	printf(".....Response len is %d\n", (int) res.resp_len);
	printf(".....%s\n", (char *) res.resp_data);
	printf("Command failed as expected\n");

	ret = cmd_get_res(&cmd2, &res2);
	if (ret != 0) {
		printf("Unable to get result\n");
		exit(1);
	}
	if((res2.sense_len != 0) || (res2.command_status != 0)){
		printf("Sense data found means error!\n");
		cmd_show_error(&res2);
		exit(1);
	}
	printf("\n.....Response len is %d\n", (int) res2.resp_len);
	printf(".....%s\n", (char *) res2.resp_data);
	printf("Command worked successfully\n\n");
	cmd_free_res(&res2);

	printf("Problem with the first command was:\n");
	cmd_show_error(&res);


	cmd_free_res(&res);

}
void create_objects(void)
{
	struct object_attrs object;
	int ret;
	struct cmd_result res;
	struct gen_osd_cmd cmd;
	uint16_t count = 10;

	/*cmd.current_drive = osd_fd;*/
	/*      OR                   */
	ret = gen_osd_select_drive(&drive_list, &cmd, drive_id);
	if (ret != 0){
		printf("Unable to select drive\n");
		exit(1);
	}

	object.oid = 0;
	object.pid = PVFS_OSD_PID;
	object.count = count;

	printf("Create the command\n");
	ret = cmd_set(&cmd, CREATE_OBJECT, &object);
	if (ret != 0 ){
		printf("Unable to set command\n");
		exit(1);
	}

	printf("Submit the create obj command\n");
	ret = cmd_submit(&cmd);
	if (ret != 0){
		printf("Submit command failed\n");
		exit(1);
	}

	ret = cmd_get_res(&cmd, &res);
	if (ret != 0) {
		printf("Unable to get result\n");
		exit(1);
	}

	if((res.sense_len != 0) || (res.command_status != 0)){
		printf("ERROR!\n");
		cmd_show_error(&res);
		cmd_free_res(&res);
		exit(1);
	}

	cmd_free_res(&res);



}

void remove_objects(void)
{
	/*Since create object with get attributes not implemented yet
	just assume we know the first object ID which we do do here*/

	struct object_attrs object;
	int ret;
	struct cmd_result res;
	struct gen_osd_cmd cmd;
	int i;

	/*cmd.current_drive = osd_fd;*/
	/*      OR                   */
	ret = gen_osd_select_drive(&drive_list, &cmd, drive_id);
	if (ret != 0){
		printf("Unable to select drive\n");
		exit(1);
	}

	object.oid = FIRST_OID;
	object.oid++;
	object.pid = PVFS_OSD_PID;
	for(i=0; i< 5; i++){
		object.oid+=1;

		printf("Create the command\n");
		ret = cmd_set(&cmd, REMOVE_OBJECT, &object);
		if (ret != 0 ){
			printf("Unable to set command\n");
			exit(1);
		}

		printf("Submit the create obj command\n");
		ret = cmd_submit(&cmd);
		if (ret != 0){
			printf("Submit command failed\n");
			exit(1);
		}

		ret = cmd_get_res(&cmd, &res);
		if (ret != 0) {
			printf("Unable to get result\n");
			exit(1);
		}

		if((res.sense_len != 0) || (res.command_status != 0)){
			printf("ERROR!\n");
			cmd_show_error(&res);
			cmd_free_res(&res);
			exit(1);
		}

		cmd_free_res(&res);

		printf("Just removed object %ld\n", object.oid);
	}


}

void write_objects(void)
{
	struct io_attrs write;
	int ret;
	struct cmd_result res;
	struct gen_osd_cmd cmd;
	int i;

	/*cmd.current_drive = osd_fd;*/
	/*      OR                   */
	ret = gen_osd_select_drive(&drive_list, &cmd, drive_id);
	if (ret != 0){
		printf("Unable to select drive\n");
		exit(1);
	}

	buf = malloc(1024);
	for(i=0; i<1024; i++){
		memset(buf+i, 'D', 1);
	}

	write.oid = FIRST_OID;
	write.pid = PVFS_OSD_PID;
	write.len = 1024;
	write.offset = 0;
	write.write_buf = buf;

	printf("Create the command\n");
	ret = cmd_set(&cmd, WRITE, &write);
	if (ret != 0 ){
		printf("Unable to set command\n");
		exit(1);
	}

	printf("Submit the create obj command\n");
	ret = cmd_submit(&cmd);
	if (ret != 0){
		printf("Submit command failed\n");
		exit(1);
	}

	ret = cmd_get_res(&cmd, &res);
	if (ret != 0) {
		printf("Unable to get result\n");
		exit(1);
	}

	if((res.sense_len != 0) || (res.command_status != 0)){
		printf("ERROR!\n");
		cmd_show_error(&res);
		cmd_free_res(&res);
		exit(1);
	}

	cmd_free_res(&res);



}

void read_objects(void)
{
	struct io_attrs read;
	int ret;
	struct cmd_result res;
	struct gen_osd_cmd cmd;
	int i;

	/*cmd.current_drive = osd_fd;*/
	/*      OR                   */
	ret = gen_osd_select_drive(&drive_list, &cmd, drive_id);
	if (ret != 0){
		printf("Unable to select drive\n");
		exit(1);
	}

	buf2 = malloc(1024);

	read.oid = FIRST_OID;
	read.pid = PVFS_OSD_PID;
	read.len = 1024;
	read.offset = 0;
	read.read_buf = buf2;

	printf("Create the command\n");
	ret = cmd_set(&cmd, READ, &read);
	if (ret != 0 ){
		printf("Unable to set command\n");
		exit(1);
	}

	printf("Submit the create obj command\n");
	ret = cmd_submit(&cmd);
	if (ret != 0){
		printf("Submit command failed\n");
		exit(1);
	}

	ret = cmd_get_res(&cmd, &res);
	if (ret != 0) {
		printf("Unable to get result\n");
		exit(1);
	}

	if((res.sense_len != 0) || (res.command_status != 0)){
		printf("ERROR!\n");
		cmd_show_error(&res);
		cmd_free_res(&res);
		exit(1);
	}

	cmd_free_res(&res);

	for (i=0; i< 1024; i++){
		if (buf[i] != buf2[i]){
			printf("Error Data Invalid!\n");
			exit(1);
		}
	}

	printf("Buffers Check Out\n");


}
int main(void)
{

	init();

	format();

	create_partition();

	create_objects();

	remove_objects();

	write_objects();

	read_objects();

	fini();


	return 0;
}
