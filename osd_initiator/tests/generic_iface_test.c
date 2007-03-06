#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "util/util.h"
#include "command.h"
#include "device.h"
#include "sense.h"
#include "generic_iface.h"

#define PVFS_OSD_PID 0x10003ULL
#define FIRST_OID  0x10000LLU;

static const int OBJ_CAPACITY = 1<<30; /* 1 GB */

static int drive_id;
static struct gen_osd_drive_list drive_list;
static char *buf;
static char *buf2;


static void init(void)
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

static void format(void)
{
	int ret, fd;
	struct osd_command command;

	printf("BEGINNING FORMAT TEST\n");

	printf("Setting FD\n");
	fd = gen_osd_select_drive(&drive_list, drive_id);
	if (fd < 0){
		printf("Unable to select drive\n");
		exit(1);
	}

	printf("Formatting active disk\n");
	printf(".....Set format command\n");
	ret = osd_command_set_format_osd(&command, OBJ_CAPACITY);
	if (ret != 0){
		printf("Couldn't set format command\n");
		exit(1);
	}
	printf(".....Submit the command\n");
	ret = osd_submit_command(fd, &command);
	if (ret != 0){
		printf("Submit command failed\n");
		exit(1);
	}
	printf(".....Get the status of the command\n");
	ret = gen_osd_cmd_getcheck_res(fd, &command);
	if (ret != 0) {
		printf("Unable to get result\n");
		exit(1);
	}

	if(command.sense_len != 0){
		printf("Sense data found!\n");
		exit(1);
	}

	if(command.status != 0){
		printf("Command FAILED!\n");
		exit(1);
	}
	printf(".....Response len is %d\n", (int) command.inlen);
	printf(".....%s\n", (char *) command.indata);

	printf("END FORMAT TEST\n");

}

static void fini(void)
{
	int ret;
	printf("Close the drive\n");
	ret = gen_osd_close_drive(&drive_list, drive_id);
	if (ret != 0){
		printf("Unable to close drive\n");
		exit(1);
	}
}

static void test_create_partition(void)
{
	int ret, fd;
	struct osd_command command;
	struct osd_command command2;

	printf("BEGIN CREATE PARTITION\n");

	printf("Setting FD for cmd non-PVFS way\n");
	fd = gen_osd_select_drive(&drive_list, drive_id);
	if (fd < 0){
		printf("Unable to select drive\n");
		exit(1);
	}

	printf("Create Partition command that should fail\n");
	ret = osd_command_set_create_partition(&command, 2);
	if (ret != 0){
		printf("Couldn't set create partition command\n");
		exit(1);
	}

	printf("Create Partition command that should work\n");
	ret = osd_command_set_create_partition(&command2, PVFS_OSD_PID);
	if (ret != 0){
		printf("Couldn't set create partition command\n");
		exit(1);
	}

	printf("Submit the commands\n");
	ret = osd_submit_command(fd, &command);
	if (ret != 0){
		printf("Submit command failed\n");
		exit(1);
	}
	ret = osd_submit_command(fd, &command2);
	if (ret != 0){
		printf("Submit command failed\n");
		exit(1);
	}

	printf("Get the status of the commands\n\n");

	ret = gen_osd_cmd_getcheck_res(fd, &command);
	if (ret != 0) {
		printf("Unable to get result\n");
		exit(1);
	}
	if(command.sense_len == 0){
		printf("No Sense data found when there should have been!\n");
		exit(1);
	}
	if(command.status == 0){
		printf("Command did not FAIL when it should!\n");
		exit(1);
	}
	printf(".....Response len is %d\n", (int) command.inlen);
	printf(".....%s\n", (char *) command.indata);
	printf("Command failed as expected\n");

	ret = gen_osd_cmd_getcheck_res(fd, &command2);
	if (ret != 0) {
		printf("Unable to get result\n");
		exit(1);
	}
	if((command2.sense_len != 0) || (command2.status != 0)){
		printf("Sense data found means error!\n");
		fputs(osd_show_sense(command2.sense, command2.sense_len), stderr);
		exit(1);
	}
	printf(".....Response len is %d\n", (int) command2.inlen);
	printf(".....%s\n", (char *) command2.indata);
	printf("Command worked successfully\n\n");

	printf("Problem with the first command was:\n");
	fputs(osd_show_sense(command.sense, command.sense_len), stderr);

	printf("END CREATE PARTITION TEST\n");

}

static void create_objects(void)
{
	int ret, fd;
	struct osd_command command;
	uint16_t count = 10;

	printf("BEGIN CREATE OBJECT TEST\n");

	fd = gen_osd_select_drive(&drive_list, drive_id);
	if (fd < 0){
		printf("Unable to select drive\n");
		exit(1);
	}

	printf("Create the command\n");
	ret = osd_command_set_create(&command, PVFS_OSD_PID, 0, count);
	if (ret != 0 ){
		printf("Unable to set command\n");
		exit(1);
	}

	printf("Submit the create obj command\n");
	ret = osd_submit_command(fd, &command);
	if (ret != 0){
		printf("Submit command failed\n");
		exit(1);
	}

	ret = gen_osd_cmd_getcheck_res(fd, &command);
	if (ret != 0) {
		printf("Unable to get result\n");
		exit(1);
	}
	if((command.sense_len != 0) || (command.status != 0)){
		printf("Sense data found means error!\n");
		fputs(osd_show_sense(command.sense, command.sense_len), stderr);
		exit(1);
	}
	printf(".....Response len is %d\n", (int) command.inlen);
	printf(".....%s\n", (char *) command.indata);
	printf("Command worked successfully\n\n");

	printf("END CREATE OBJECT TEST\n");
}

static void remove_objects(void)
{
	/*Since create object with get attributes not implemented yet
	just assume we know the first object ID which we do do here*/

	int ret, fd;
	struct osd_command command;
	int i;
	uint64_t oid, pid;

	fd = gen_osd_select_drive(&drive_list, drive_id);
	if (fd < 0){
		printf("Unable to select drive\n");
		exit(1);
	}

	oid = FIRST_OID;
	oid++;
	pid = PVFS_OSD_PID;
	for(i=0; i< 5; i++){
		oid+=1;

		printf("Create the command\n");
		ret = osd_command_set_remove(&command, pid, oid);
		if (ret != 0 ){
			printf("Unable to set command\n");
			exit(1);
		}

		printf("Submit the create obj command\n");
		ret = osd_submit_command(fd, &command);
		if (ret != 0){
			printf("Submit command failed\n");
			exit(1);
		}

		ret = gen_osd_cmd_getcheck_res(fd, &command);
		if (ret != 0) {
			printf("Unable to get result\n");
			exit(1);
		}

		if((command.sense_len != 0) || (command.status != 0)){
			printf("Sense data found means error!\n");
			fputs(osd_show_sense(command.sense, command.sense_len), stderr);
			exit(1);
		}

		printf("Just removed object %ld\n", oid);
	}
	printf("\n");
}

static void write_objects(void)
{
	int ret, fd;
	struct osd_command command;
	int i;
	uint64_t oid, pid;
	size_t len;
	size_t offset;

	fd = gen_osd_select_drive(&drive_list, drive_id);
	if (fd < 0){
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

	printf("%s: Create the command\n", __func__);
	ret = osd_command_set_write(&command, pid, oid, len, offset);
	if (ret != 0 ){
		printf("Unable to set command\n");
		exit(1);
	}
	command.outdata = buf;
	command.outlen = len;

	printf("Submit the write command\n");
	ret = osd_submit_command(fd, &command);
	if (ret != 0){
		printf("Submit command failed\n");
		exit(1);
	}

	ret = gen_osd_cmd_getcheck_res(fd, &command);
	if (ret != 0) {
		printf("Unable to get result\n");
		exit(1);
	}

	if((command.sense_len != 0) || (command.status != 0)){
		printf("Sense data found means error!\n");
		fputs(osd_show_sense(command.sense, command.sense_len), stderr);
		exit(1);
	}


}

static void read_objects(void)
{
	int ret, fd;
	struct osd_command command;
	int i;
	uint64_t oid, pid;
	size_t len, offset;

	fd = gen_osd_select_drive(&drive_list, drive_id);
	if (fd < 0){
		printf("Unable to select drive\n");
		exit(1);
	}

	buf2 = malloc(1024);

	oid = FIRST_OID;
	pid = PVFS_OSD_PID;
	len = 1024;
	offset = 0;

	printf("Create the command\n");
	ret = osd_command_set_read(&command, pid, oid, len, offset);
	if (ret != 0 ){
		printf("Unable to set command\n");
		exit(1);
	}
	command.indata = buf2;
	command.inlen_alloc = len;

	printf("Submit the read command\n");
	ret = osd_submit_command(fd, &command);
	if (ret != 0){
		printf("Submit command failed\n");
		exit(1);
	}

	ret = gen_osd_cmd_getcheck_res(fd, &command);
	if (ret != 0) {
		printf("Unable to get result\n");
		exit(1);
	}

	if((command.sense_len != 0) || (command.status != 0)){
		printf("Sense data found means error!\n");
		fputs(osd_show_sense(command.sense, command.sense_len), stderr);
		exit(1);
	}


	for (i=0; i< 1024; i++){
		if (buf[i] != buf2[i]){
			printf("Error Data Invalid!\n");
			exit(1);
		}
	}

	printf("Buffers Check Out\n\n");
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
