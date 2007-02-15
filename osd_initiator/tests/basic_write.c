/*
 * Test the chardev transport to the kernel.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <scsi/scsi.h>

#include <stdint.h>
#include <sys/types.h>

#include "util/util.h" 
#include "kernel_interface.h"
#include "cdb_manip.h"
#include "user_interface.h"

int main(int argc, char *argv[])
{
	int cdb_len = OSD_CDB_SIZE;
	int fd, i;  
	char *buf;
	int ret;
	int len;
		
	set_progname(argc, argv); 
	fd = dev_osd_open("/dev/sua");
	if (fd < 0) {
		error_errno("%s: open /dev/sua", __func__);
		return 1;
	}

	inquiry(fd);
	flush_osd(fd, cdb_len);  /*this is a no op no need to flush, on target files are
						opened read/written and closed each time so 
						objects are in a sense flushed every time*/

	ret = format_osd(fd, cdb_len, 1<<30);  /*1GB*/
	if(ret != 0)
		return ret;
	ret = create_object(fd, cdb_len, FIRST_USER_PARTITION, FIRST_USER_OBJECT, 1); 
	if(ret != 0)
		return ret;
	
	buf = malloc(BUFSIZE);
	memset(buf, '\0', BUFSIZE);
	strcpy(buf, "The Rain in Spain");
	
	printf("Going to write: %s\n", buf);
	len = strlen(buf) + 1;
	ret = write_osd(fd, cdb_len, FIRST_USER_PARTITION, FIRST_USER_OBJECT, len, 0, buf); 	
	if(ret != 0)
		return ret;
	
	memset(buf, '\0', BUFSIZE);
	printf("Buf should be cleared: %s\n", buf);
	ret = read_osd(fd, cdb_len, FIRST_USER_PARTITION, FIRST_USER_OBJECT, len, 0, buf);
	if(ret != 0)
		return ret;
	printf("Read back: %s\n", buf);
	
	/*create a bunch of objects*/
	for(i=0; i< 10; i+=1){
		ret = create_object(fd, cdb_len, FIRST_USER_PARTITION+1+i, FIRST_USER_OBJECT+1+i, 1); 
		if(ret != 0)
			return ret;
	}
	/*remove some of those objects*/
	for(i=0; i< 10; i+=2){
		ret = remove_object(fd, cdb_len, FIRST_USER_PARTITION+1+i, FIRST_USER_OBJECT+1+i);
		if(ret != 0)
			return ret;
	}
	
	dev_osd_close(fd);
	return 0;
}
