/*
 * General test of the interface.
 *
 * Copyright (C) 2007 OSD Team <pvfs-osd@osc.edu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>

#include "osd-util/osd-util.h"
#include "command.h"
#include "device.h"
#include "drivelist.h"
#include "sense.h"

#define FIRST_PID  USEROBJECT_PID_LB
#define FIRST_OID  USEROBJECT_OID_LB

#define FULL_TEST  /*comment out for simple test - create 1 part and 1 obj
			//~ no read or write, etc*/

static int fd;
static char *buf;
static char *buf2;

static void init(void)
{
	int ret;
	struct osd_drive_description *drive_list;
	int num_drives;

	printf("Initialization\n");
	ret = osd_get_drive_list(&drive_list, &num_drives);
	if (ret < 0) {
		osd_error_errno("%s: get drive list", __func__);
		exit(1);
	}
	if (num_drives == 0) {
		osd_error("%s: No drives found: %d", __func__, num_drives);
		exit(1);
	}

	printf("Found %d Drives Now open drive #0\n", num_drives);
	fd = open(drive_list[0].chardev, O_RDWR);
	if (fd < 0) {
		osd_error_errno("%s: open %s", __func__, drive_list[0].chardev);
		exit(1);
	}
}

static void format(void)
{
	int ret;
	struct osd_command command;
	const int OBJ_CAPACITY = 1<<30;  /* 1 GB */

	printf("BEGIN FORMAT TEST\n");

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
	ret = osd_wait_this_response(fd, &command);
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
	ret = close(fd);
	if (ret) {
		osd_error_errno("%s: close drive", __func__);
		exit(1);
	}
}

static void test_create_partition(void)
{
	int ret;
	struct osd_command command;
	struct osd_command command2;

	printf("BEGIN CREATE PARTITION\n");

#ifdef FULL_TEST
	printf("Create Partition command that should fail\n");
	ret = osd_command_set_create_partition(&command, 2);
	if (ret != 0){
		printf("Couldn't set create partition command\n");
		exit(1);
	}
#endif
	printf("Create Partition command that should work\n");
	ret = osd_command_set_create_partition(&command2, FIRST_PID);
	if (ret != 0){
		printf("Couldn't set create partition command\n");
		exit(1);
	}

#ifdef FULL_TEST
	printf("Submit the commands\n");
	ret = osd_submit_command(fd, &command);
	if (ret != 0){
		printf("Submit command failed\n");
		exit(1);
	}
#endif
	ret = osd_submit_command(fd, &command2);
	if (ret != 0){
		printf("Submit command failed\n");
		exit(1);
	}

	printf("Get the status of the commands\n\n");
#ifdef FULL_TEST
	ret = osd_wait_this_response(fd, &command);
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
#endif
	ret = osd_wait_this_response(fd, &command2);
	if (ret != 0) {
		printf("Unable to get result\n");
		exit(1);
	}
	if((command2.sense_len != 0) || (command2.status != 0)){
		printf("Sense data found means error!\n");
		fputs(osd_sense_as_string(command2.sense, command2.sense_len), stderr);
		exit(1);
	}
	printf(".....Response len is %d\n", (int) command2.inlen);
	printf(".....%s\n", (char *) command2.indata);
	printf("Command worked successfully\n\n");

	printf("Problem with the first command was:\n");
	fputs(osd_sense_as_string(command.sense, command.sense_len), stderr);

	printf("END CREATE PARTITION TEST\n");

}

static void create_objects(void)
{
	int ret;
	struct osd_command command;

#ifdef FULL_TEST
	uint16_t count = 10;
#else
	uint16_t count = 1;
#endif

	printf("BEGIN CREATE OBJECT TEST\n");

	printf("Create the command\n");
	ret = osd_command_set_create(&command, FIRST_PID, 0, count);
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

	ret = osd_wait_this_response(fd, &command);
	if (ret != 0) {
		printf("Unable to get result\n");
		exit(1);
	}
	if((command.sense_len != 0) || (command.status != 0)){
		printf("Sense data found means error!\n");
		fputs(osd_sense_as_string(command.sense, command.sense_len), stderr);
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

	printf("BEGIN REMOVE OBJECTS TEST\n");

	int ret;
	struct osd_command command;
	int i;
	uint64_t oid, pid;

	oid = FIRST_OID;
	oid++;
	pid = FIRST_PID;
	for(i=0; i< 5; i++){
		oid+=1;

		printf("Create the command\n");
		ret = osd_command_set_remove(&command, pid, oid);
		if (ret != 0 ){
			printf("Unable to set command\n");
			exit(1);
		}

		printf("Submit the remove obj command\n");
		ret = osd_submit_command(fd, &command);
		if (ret != 0){
			printf("Submit command failed\n");
			exit(1);
		}

		ret = osd_wait_this_response(fd, &command);
		if (ret != 0) {
			printf("Unable to get result\n");
			exit(1);
		}

		if((command.sense_len != 0) || (command.status != 0)){
			printf("Sense data found means error!\n");
			fputs(osd_sense_as_string(command.sense, command.sense_len), stderr);
			exit(1);
		}

		printf("Just removed object %ld\n", oid);
	}
	printf("END REMOVE OBJECTS TEST\n");
	printf("\n");
}

static void write_objects(void)
{
	printf("BEGIN WRITE OBJECTS TEST\n");
	int ret;
	struct osd_command command;
	int i;
	uint64_t oid, pid;
	size_t len;
	size_t offset;

	buf = malloc(1024);
	for(i=0; i<1024; i++){
		memset(buf+i, 'D', 1);
	}

	oid = FIRST_OID;
	pid = FIRST_PID;
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

	ret = osd_wait_this_response(fd, &command);
	if (ret != 0) {
		printf("Unable to get result\n");
		exit(1);
	}

	if((command.sense_len != 0) || (command.status != 0)){
		printf("Sense data found means error!\n");
		fputs(osd_sense_as_string(command.sense, command.sense_len), stderr);
		exit(1);
	}


	printf("END WRITE OBJECTS TEST\n");
}

static void read_objects(void)
{
	printf("BEGIN READ OBJECTS TEST\n");
	int ret;
	struct osd_command command;
	int i;
	uint64_t oid, pid;
	size_t len, offset;

	buf2 = malloc(1024);

	oid = FIRST_OID;
	pid = FIRST_PID;
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

	ret = osd_wait_this_response(fd, &command);
	if (ret != 0) {
		printf("Unable to get result\n");
		exit(1);
	}

	if((command.sense_len != 0) || (command.status != 0)){
		printf("Sense data found means error!\n");
		fputs(osd_sense_as_string(command.sense, command.sense_len), stderr);
		exit(1);
	}


	for (i=0; i< 1024; i++){
		if (buf[i] != buf2[i]){
			printf("Error Data Invalid!\n");
			exit(1);
		}
	}

	printf("Buffers Check Out\n\n");
	printf("END READ OBJECTS TEST\n");
}

static void create_collection(void)
{
	int ret;
	struct osd_command command;


	printf("BEGIN CREATE COLLECTION TEST\n");

	printf("Create the command\n");
	ret = osd_command_set_create_collection(&command, FIRST_PID, FIRST_OID+1);
	if (ret != 0 ){
		printf("Unable to set command\n");
		exit(1);
	}

	printf("Submit the command\n");
	ret = osd_submit_command(fd, &command);
	if (ret != 0){
		printf("Submit command failed\n");
		exit(1);
	}

	ret = osd_wait_this_response(fd, &command);
	if (ret != 0) {
		printf("Unable to get result\n");
		exit(1);
	}
	if((command.sense_len != 0) || (command.status != 0)){
		printf("Sense data found means error!\n");
		fputs(osd_sense_as_string(command.sense, command.sense_len), stderr);
		//~ exit(1);
	}
	printf(".....Response len is %d\n", (int) command.inlen);
	printf(".....%s\n", (char *) command.indata);
	printf("Command worked successfully\n\n");

	printf("END CREATE OBJECT TEST\n");


}

int main(void)
{
	init();
	format();
	test_create_partition();
	create_objects();
	create_collection();
	#ifdef FULL_TEST
		remove_objects();
		write_objects();
		read_objects();
	#endif
	fini();
	return 0;
}
