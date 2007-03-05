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
void test_create_partition(void);
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
	int ret;
	struct gen_osd_cmd cmd;

	printf("BEGINNING FORMAT TEST\n");

	printf("Setting FD (could set cmd.current_drive directly ie PVFS)\n");
	ret = gen_osd_select_drive(&drive_list, &cmd, drive_id);
	if (ret != 0){
		printf("Unable to select drive\n");
		exit(1);
	}

	printf("Formatting active disk\n");
	printf(".....Set format command\n");
	ret = gen_osd_set_format(&(cmd.command), OBJ_CAPACITY);
	if (ret != 0){
		printf("Couldn't set format command\n");
		exit(1);
	}
	printf(".....Submit the command\n");
	ret = gen_osd_cmd_submit(cmd.current_drive, &(cmd.command));
	if (ret != 0){
		printf("Submit command failed\n");
		exit(1);
	}
	printf(".....Get the status of the command\n");
	ret = gen_osd_cmd_getcheck_res(cmd.current_drive, &(cmd.command));
	if (ret != 0) {
		printf("Unable to get result\n");
		exit(1);
	}

	if(cmd.command.sense_len != 0){
		printf("Sense data found!\n");
		exit(1);
	}

	if(cmd.command.status != 0){
		printf("Command FAILED!\n");
		exit(1);
	}
	printf(".....Response len is %d\n", (int) cmd.command.inlen);
	printf(".....%s\n", (char *) cmd.command.indata);

	printf("END FORMAT TEST\n");

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

void test_create_partition(void)
{
	int ret;
	struct gen_osd_cmd cmd;
	struct gen_osd_cmd cmd2;
	int osd_fd;

	printf("BEGIN CREATE PARTITION\n");

	printf("Setting FD for cmd non-PVFS way\n");
	ret = gen_osd_select_drive(&drive_list, &cmd, drive_id);
	if (ret != 0){
		printf("Unable to select drive\n");
		exit(1);
	}

	osd_fd = cmd.current_drive;
	cmd2.current_drive = osd_fd;

	printf("Create Partition command that should fail\n");
	ret = gen_osd_set_create_partition(&(cmd.command), 2);
	if (ret != 0){
		printf("Couldn't set create partition command\n");
		exit(1);
	}

	printf("Create Partition command that should work\n");
	ret = gen_osd_set_create_partition(&(cmd2.command), PVFS_OSD_PID);
	if (ret != 0){
		printf("Couldn't set create partition command\n");
		exit(1);
	}

	printf("Submit the commands\n");
	ret = gen_osd_cmd_submit(cmd.current_drive, &(cmd.command));
	if (ret != 0){
		printf("Submit command failed\n");
		exit(1);
	}
	ret = gen_osd_cmd_submit(cmd2.current_drive, &(cmd2.command));
	if (ret != 0){
		printf("Submit command failed\n");
		exit(1);
	}

	printf("Get the status of the commands\n\n");

	ret = gen_osd_cmd_getcheck_res(cmd.current_drive, &(cmd.command));
	if (ret != 0) {
		printf("Unable to get result\n");
		exit(1);
	}
	if(cmd.command.sense_len == 0){
		printf("No Sense data found when there should have been!\n");
		exit(1);
	}
	if(cmd.command.status == 0){
		printf("Command did not FAIL when it should!\n");
		exit(1);
	}
	printf(".....Response len is %d\n", (int) cmd.command.inlen);
	printf(".....%s\n", (char *) cmd.command.indata);
	printf("Command failed as expected\n");

	ret = gen_osd_cmd_getcheck_res(cmd2.current_drive, &(cmd2.command));
	if (ret != 0) {
		printf("Unable to get result\n");
		exit(1);
	}
	if((cmd2.command.sense_len != 0) || (cmd2.command.status != 0)){
		printf("Sense data found means error!\n");
		gen_osd_cmd_show_error(&(cmd2.command));
		exit(1);
	}
	printf(".....Response len is %d\n", (int) cmd2.command.inlen);
	printf(".....%s\n", (char *) cmd2.command.indata);
	printf("Command worked successfully\n\n");

	printf("Problem with the first command was:\n");
	gen_osd_cmd_show_error(&(cmd.command));

	printf("END CREATE PARTITION TEST\n");

}
void create_objects(void)
{
	int ret;
	struct gen_osd_cmd cmd;
	uint16_t count = 10;

	printf("BEGIN CREATE OBJECT TEST\n");

	ret = gen_osd_select_drive(&drive_list, &cmd, drive_id);
	if (ret != 0){
		printf("Unable to select drive\n");
		exit(1);
	}

	printf("Create the command\n");
	ret = gen_osd_set_create_object(&(cmd.command), PVFS_OSD_PID, 0, count);
	if (ret != 0 ){
		printf("Unable to set command\n");
		exit(1);
	}

	printf("Submit the create obj command\n");
	ret = gen_osd_cmd_submit(cmd.current_drive, &(cmd.command));
	if (ret != 0){
		printf("Submit command failed\n");
		exit(1);
	}

	ret = gen_osd_cmd_getcheck_res(cmd.current_drive, &(cmd.command));
	if (ret != 0) {
		printf("Unable to get result\n");
		exit(1);
	}
	if((cmd.command.sense_len != 0) || (cmd.command.status != 0)){
		printf("Sense data found means error!\n");
		gen_osd_cmd_show_error(&(cmd.command));
		exit(1);
	}
	printf(".....Response len is %d\n", (int) cmd.command.inlen);
	printf(".....%s\n", (char *) cmd.command.indata);
	printf("Command worked successfully\n\n");

	printf("END CREATE OBJECT TEST\n");


}

void remove_objects(void)
{
	/*Since create object with get attributes not implemented yet
	just assume we know the first object ID which we do do here*/

	int ret;
	struct gen_osd_cmd cmd;
	int i;
	uint64_t oid, pid;

	ret = gen_osd_select_drive(&drive_list, &cmd, drive_id);
	if (ret != 0){
		printf("Unable to select drive\n");
		exit(1);
	}

	oid = FIRST_OID;
	oid++;
	pid = PVFS_OSD_PID;
	for(i=0; i< 5; i++){
		oid+=1;

		printf("Create the command\n");
		ret = gen_osd_set_remove_object(&(cmd.command), pid, oid);
		if (ret != 0 ){
			printf("Unable to set command\n");
			exit(1);
		}

		printf("Submit the create obj command\n");
		ret = gen_osd_cmd_submit(cmd.current_drive, &(cmd.command));
		if (ret != 0){
			printf("Submit command failed\n");
			exit(1);
		}

		ret = gen_osd_cmd_getcheck_res(cmd.current_drive, &(cmd.command));
		if (ret != 0) {
			printf("Unable to get result\n");
			exit(1);
		}

		if((cmd.command.sense_len != 0) || (cmd.command.status != 0)){
			printf("Sense data found means error!\n");
			gen_osd_cmd_show_error(&(cmd.command));
			exit(1);
		}

		printf("Just removed object %ld\n", oid);
	}


}

void write_objects(void)
{
	int ret;
	struct gen_osd_cmd cmd;
	int i;
	uint64_t oid, pid;
	size_t len;
	size_t offset;

	ret = gen_osd_select_drive(&drive_list, &cmd, drive_id);
	if (ret != 0){
		printf("Unable to select drive\n");
		exit(1);
	}

	buf = malloc(1024);
	for(i=0; i<1024; i++){
		memset(buf+i, 'D', 1);
	}

	oid = FIRST_OID;
	pid = PVFS_OSD_PID;
	len = 1024;
	offset = 0;

	printf("Create the command\n");
	ret = gen_osd_write_object(&(cmd.command), pid, oid, buf, len, offset);
	if (ret != 0 ){
		printf("Unable to set command\n");
		exit(1);
	}

	printf("Submit the create obj command\n");
	ret = gen_osd_cmd_submit(cmd.current_drive, &(cmd.command));
	if (ret != 0){
		printf("Submit command failed\n");
		exit(1);
	}

	ret = gen_osd_cmd_getcheck_res(cmd.current_drive, &(cmd.command));
	if (ret != 0) {
		printf("Unable to get result\n");
		exit(1);
	}

	if((cmd.command.sense_len != 0) || (cmd.command.status != 0)){
		printf("Sense data found means error!\n");
		gen_osd_cmd_show_error(&(cmd.command));
		exit(1);
	}


}

void read_objects(void)
{
	int ret;
	struct gen_osd_cmd cmd;
	int i;
	uint64_t oid, pid;
	size_t len, offset;

	ret = gen_osd_select_drive(&drive_list, &cmd, drive_id);
	if (ret != 0){
		printf("Unable to select drive\n");
		exit(1);
	}

	buf2 = malloc(1024);

	oid = FIRST_OID;
	pid = PVFS_OSD_PID;
	len = 1024;
	offset = 0;

	printf("Create the command\n");
	ret = gen_osd_read_object(&(cmd.command), pid, oid, buf, len, offset);
	if (ret != 0 ){
		printf("Unable to set command\n");
		exit(1);
	}

	printf("Submit the create obj command\n");
	ret = gen_osd_cmd_submit(cmd.current_drive, &(cmd.command));
	if (ret != 0){
		printf("Submit command failed\n");
		exit(1);
	}

	ret = gen_osd_cmd_getcheck_res(cmd.current_drive, &(cmd.command));
	if (ret != 0) {
		printf("Unable to get result\n");
		exit(1);
	}

	if((cmd.command.sense_len != 0) || (cmd.command.status != 0)){
		printf("Sense data found means error!\n");
		gen_osd_cmd_show_error(&(cmd.command));
		exit(1);
	}


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

	test_create_partition();

	create_objects();

	remove_objects();

	write_objects();

	read_objects();

	fini();


	return 0;
}
