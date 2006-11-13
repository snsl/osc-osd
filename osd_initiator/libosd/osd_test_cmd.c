/*************************************************************
** Copyright 2004, 2005. IBM Corp.** All Rights Reserved.***
**************************************************************
** Redistribution and use in source and binary forms, with or without
** modifications, are permitted provided that the following conditions 
** are met:
** 
** i.  Redistribution of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**
** ii. Redistribution in binary form must reproduce the above copyright 
**     notice, this list of conditions and the following disclaimer in the 
**     documentation and/or other materials provided with the distribution.
**
** iii.Neither the name(s) of IBM Corp.** nor the names of its/their
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
*******************************************************************
** THIS SOFTWARE IS PROVIDED BY IBM CORP. AND CONTRIBUTORS "AS IS" 
** AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING , BUT NOT LIMITED
** TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A 
** PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE IBM
** CORP. OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
** (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
** OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
** STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
** ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED 
** OF THE POSSIBILITY OF SUCH DAMAGE.
** 
*****************************************************************************
*****************************************************************************/

#ifndef __KERNEL__
#include <string.h>
#endif

#include "osd_ops.h"
#include "osd_ops_utils.h"
#include "osd_attr_defs.h"

#define OSD_TEST_MAX_PARTITIONS 6
#define OSD_TEST_MAX_OBJECTS 3
#define DATA_LEN 4096
#define LIST_BUF_LEN 200
#define LUN_SIZE (1000000000) //1GB

static void free_bufs(char *data_out_p,
		      char *data_in_p,
		      char *list_buf_p)
{
    if (data_out_p) os_free(data_out_p);
    if (data_in_p) os_free(data_in_p);
    if (list_buf_p) os_free(list_buf_p);
}

int test_osd_commands(osd_dev_handle_t dev)
{
    uint64_t part_id[OSD_TEST_MAX_PARTITIONS];
    uint64_t oid[OSD_TEST_MAX_OBJECTS];
    uint64_t obj_len = 0;
    uint64_t * list_arr; //this ptr set by initiator functions
                         //to a place within other data buffers
    uint32_t entries;
    char *data_out_p;
    char *data_in_p;
    char *list_buf_p;
    char * test_str = "this is a testsssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssst\0";
    int i, rc;

    //since data buffers are larger then kernel stack size - they cannot be on stack
    //NOTE: every code path leaving this function should free these buffers
    data_in_p  = os_malloc(DATA_LEN+4);
    data_out_p = os_malloc(DATA_LEN+4);
    list_buf_p = os_malloc(LIST_BUF_LEN+32);

    if ( ( ! data_in_p) || ( ! data_out_p) || ( ! list_buf_p) ) {
	OSD_OPS_ERROR("os_malloc() failed!\n");
	free_bufs(data_out_p, data_in_p, list_buf_p);
	return -1;
    }
    
    memset(data_in_p,0,DATA_LEN);
    memset(data_out_p,0,DATA_LEN);

    if ((strlen(test_str) >= (DATA_LEN-1))) {
	test_str[DATA_LEN-1]='\0';
    }

    rc=osd_format(dev,LUN_SIZE);
    if(rc != 0) {
	OSD_OPS_ERROR("failed to format lun\n");
	free_bufs(data_out_p, data_in_p, list_buf_p);
	return -1;
    }
    
    /* create partition */
    for(i = 0; i < OSD_TEST_MAX_PARTITIONS; i++) {
        part_id[i]=0x10001+i;

        rc = osd_create_partition(dev, &part_id[i]);
        if(rc != 0) {
            OSD_OPS_ERROR("failed to create partition\n");
	    free_bufs(data_out_p, data_in_p, list_buf_p);
	    return -1;
	}
    }

    rc = osd_list(dev, 0, list_buf_p, &entries, &list_arr, LIST_BUF_LEN);
    if (rc != 0) {
        OSD_OPS_ERROR("failed list.\n");
	free_bufs(data_out_p, data_in_p, list_buf_p);
        return -1;
    }
    if (entries!=OSD_TEST_MAX_PARTITIONS) {
        OSD_OPS_ERROR("expected %d entries in list, got %d\n",
		    OSD_TEST_MAX_PARTITIONS,(int)entries);
	free_bufs(data_out_p, data_in_p, list_buf_p);
        return -1;
    }


    /* list with smal buffer is commented out until oc supports it
     *
    //test list with small din buf to use continuing commands
    if ( 0x28 > LIST_BUF_LEN) {
        OSD_OPS_ERROR("LIST_BUF_LEN too small!!!\n");
    }
    rc = osd_list(dev, 0, list_buf_p, &entries, &list_arr, 0x28);
    if(rc != 0) {
        OSD_OPS_ERROR("failed list.\n");
	free_bufs(data_out_p, data_in_p, list_buf_p);
        return -1;
    }
    if (entries!=OSD_TEST_MAX_PARTITIONS) {
        OSD_OPS_ERROR("expected %d entries in list, gopt %d\n",
		    OSD_TEST_MAX_PARTITIONS,(int)entries);
	free_bufs(data_out_p, data_in_p, list_buf_p);
        return -1;
    }
    *
    * end of list-with_small_buf test */

    /* create objects */
    for(i = 0; i < OSD_TEST_MAX_OBJECTS; i++) { 
	oid[i]=0x10001+i;
        rc = osd_create_object(dev, part_id[0], &oid[i]);
        
        if(rc != 0) {
            OSD_OPS_ERROR("failed to create object\n");
	    free_bufs(data_out_p, data_in_p, list_buf_p);
	    return -1;
        }
    }

/* An attempt to recreate Ralph's problem:
   write a 64 MB object and make a large clear in it before going on with other cmds
*/
    {
	uint64_t big_len = 35*1024;

	PRINT("creating a ~%lld bytes object\n",big_len);
	obj_len=0;
	for (i=0;obj_len<big_len;i++) {
	    rc = osd_write(dev, part_id[0], oid[0], 
			   data_out_p, (long)i*DATA_LEN, DATA_LEN, &obj_len);
	    
	    if(rc != 0) {
		OSD_OPS_ERROR("failed to write to object\n");
		free_bufs(data_out_p, data_in_p, list_buf_p);
		return -1;
	    }
	}
	PRINT("created a %lld bytes object, now clearing all but first %d\n",
	      obj_len, DATA_LEN);
	
	rc = osd_clear(dev, part_id[0], oid[0], 
		       DATA_LEN, big_len-DATA_LEN);
	
	if(rc != 0) {
	OSD_OPS_ERROR("failed to clear object\n");
	free_bufs(data_out_p, data_in_p, list_buf_p);
	return -1;
	}
    }
    
    
/* Now we should have ralph's condition...*/


    /* write */
    strncpy(data_out_p, test_str, DATA_LEN);
    rc = osd_write(dev, part_id[0], oid[0], 
		   data_out_p, 0, DATA_LEN, &obj_len);
    if(rc != 0) {
        OSD_OPS_ERROR("failed to write to object\n");
	free_bufs(data_out_p, data_in_p, list_buf_p);
        return -1;
    }
    
    PRINT("Write success object length = %Lu \n", obj_len);

    /* read */
    
    rc = osd_read(dev, part_id[0], oid[0], 0, data_in_p, DATA_LEN);
    if(rc != 0) {
        OSD_OPS_ERROR("failed to read from object\n");
	free_bufs(data_out_p, data_in_p, list_buf_p);
        return -1;
    }
    
    if(strncmp(test_str, data_in_p, strlen(test_str))) {
        OSD_OPS_ERROR("diff between read and write failed\n");
	free_bufs(data_out_p, data_in_p, list_buf_p);
        return -1;
    }
    
    PRINT("finished osd read \n");

    /*list */

    rc = osd_list(dev, part_id[0], list_buf_p, &entries, &list_arr, LIST_BUF_LEN);
    if(rc != 0) {
        OSD_OPS_ERROR("failed list.\n");
	free_bufs(data_out_p, data_in_p, list_buf_p);
        return -1;
    }
    if (entries!=OSD_TEST_MAX_OBJECTS) {
        OSD_OPS_ERROR("expected %d entries in list, gopt %d\n",
		    OSD_TEST_MAX_OBJECTS,(int)entries);
	free_bufs(data_out_p, data_in_p, list_buf_p);
        return -1;
    }

    /* remove objects */
    for(i = 0; i < OSD_TEST_MAX_OBJECTS; i++) {
        rc = osd_remove(dev, part_id[0],oid[i]);
        if(rc != 0) {
            OSD_OPS_ERROR("failed to remove object\n");
	    free_bufs(data_out_p, data_in_p, list_buf_p);
            return -1;
        }
    }

    //remove partitions
    for(i = 0; i < OSD_TEST_MAX_PARTITIONS; i++) {
        rc = osd_remove_partition(dev, part_id[i]);
        if(rc != 0) {
            OSD_OPS_ERROR("failed to remove object\n");
	    free_bufs(data_out_p, data_in_p, list_buf_p);
            return -1;
        }
    }

    osd_set_root_key(dev, (uint8_t*) data_out_p, (uint8_t*) "ROOTKEY");

    free_bufs(data_out_p, data_in_p, list_buf_p);
    return 0;
}

#define VPD_PAGE_MAX_LEN 0xff

uint64_t test_osd_get_id(osd_dev_handle_t dev)
{
    int rc;
    char data[VPD_PAGE_MAX_LEN] = {0};
    
    rc = osd_inquiry_83(dev, data, VPD_PAGE_MAX_LEN);

    if (rc != 0) {
        OSD_OPS_ERROR("failed to read device ID\n");
        return 0;
    }

    return extract_id_from_inquiry_data(data);
    
}
