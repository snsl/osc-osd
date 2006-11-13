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
#include <memory.h>
#endif

#include "osd_ops.h"
#include "osd_attr_defs.h" /* include all standard attribute definitions */
#include "sec_admin.h"
#include "osd_ops_utils.h"

void wait_req(struct osd_req_t *req,int timeout,int retries,
              int non_osd_cmd,
	      uint16_t service_action, 
	      const sec_cmd_params_t *sec_params_p
	      )
{
    uint8_t credential_a[OSD_CREDENTIAL_SIZE];
    uint8_t key_a[OSD_CRYPTO_VAL_SIZE],audit_a[OSD_CRYPTO_VAL_SIZE];
    uint8_t sys_id_a[20];
    array12_t discriminator;

    if (! non_osd_cmd) {
        osd_assert(NULL!=sec_params_p);

        memset(sys_id_a,0,sizeof(sys_id_a));
        memset(key_a,0,sizeof(key_a));
	memset(audit_a,0,sizeof(audit_a));
        memset(discriminator.b,0,sizeof(discriminator.b));
        
	sec_generate_credential(credential_a,
				1, //1 service actions
				&service_action, //service actions array
				sec_params_p,
				OSD_NOSEC, //security method to use
				sys_id_a, //system id of target
				0, //key version
				key_a, //key to use
				(uint64_t)0x100000000000LL, //expiration time
				audit_a, //audit
				discriminator,
				0, //create time - 0=don't check
				0); //policy acces tag, 0=don't check
    }
				       
#ifdef TEST_AIO
    ctx = create_osd_ctx(1);
    if(ctx == NULL) {
        OSD_ERROR("Create ctx failed\n");
        return;
    }
    
    os_init_completion(&completion);
    
    do_osd_req(req, ctx, req_done, &completion, credential_a, NULL);

    if(get_completed_osd_req(ctx, 1, 1) != 1) {
        OSD_ERROR("get completed failed\n");
    }
    
    /* in kernel mode get_completed_osd_req is empty we need to wait 
       for the req to end */
    PRINT("wait for completion %p\n", &completion);
    os_wait_for_completion(&completion);
    PRINT("woke up\n");

    destroy_osd_ctx(ctx);
#else
    wait_osd_req(req,timeout,retries, credential_a, NULL);
#endif
}

static int extract_attribute_from_list(struct osd_req_t * req,
                                       uint32_t page_num, 
                                       uint32_t attr_num, 
                                       uint16_t attr_len,
                                       unsigned char * attr_val)
{
    uint32_t p_num,a_num;
    uint16_t got_len;
    uint8_t *val_buf;
    int rc;
    
    rc = init_extract_attr_from_req(req);
    if(rc != 0) {
        OSD_OPS_ERROR("error initializing extract attributes\n");
        return -1;
    }
    
    rc = extract_next_attr_from_req(req, &p_num, &a_num, &got_len, &val_buf);
    
    while( rc == 0) {
        if ((p_num==page_num) &&
            (a_num==attr_num) ) {
	    if (got_len==OSD_ATTR_UNDEFINED_VAL_LENGTH) {
		PRINT("value of page %x attr %x is undefined on target.\n",
		      page_num, attr_num);
 		return -1;
	    }

            if(got_len < attr_len) {
                attr_len = got_len;
            }
            
            PRINT("Found attr: page %x attr %x got %d bytes\n", 
                  page_num, attr_num,
                  got_len);
            memcpy(attr_val, val_buf, attr_len);
            return 0;
        }
        
        rc = extract_next_attr_from_req(req, &p_num, &a_num, &got_len, &val_buf);
        
    }
    OSD_OPS_ERROR("page 0x%x attr 0x%x not found in list\n",
                   page_num,attr_num);
    return -1;
}

int extract_attribute(uint32_t page, uint32_t index, uint16_t len,
                      uint8_t *data, unsigned length, void *val) {
  int i = 0;

  for (i=0; i<length;) {
      if (NTOHL(*(uint32_t *)(data+i))!=page) {
      OSD_OPS_ERROR("page mismatch: got 0x%x, expected 0x%x\n", NTOHL(*(uint32_t *)(data+i)), page);
      return -1;
    }
    i+=4;
    if (NTOHL(*(uint32_t *)(data+i))!=index) {
      OSD_OPS_ERROR("index mismatch\n");
      return -1;
    }
    i+=4;
    if (NTOHS(*(uint16_t *)(data+i))!=len) {
      OSD_OPS_ERROR("len mismatch\n");
      return -1;
    }
    i+=2;
    PRINT("page 0x%x, index %u, len %u\n", page, index, len);
    memcpy(val, data+i, len);
    i+=len;
  }
  PRINT("parsed %i bytes\n", i);
  return i;
}

int osd_create_partition(osd_dev_handle_t dev, uint64_t *part_id)
{
    struct osd_req_t * req;
    uint32_t page = 0,attr = 0;
    osd_result_t res;
    
    req = prepare_osd_create_partition(dev, *part_id);
        
    if(req == NULL)  {
        OSD_OPS_ERROR("failed to create partition\n");
        return -1;
    }
    if(*part_id == 0) {
        page=OSD_PAGE_NUM_CUR_CMD; 
        attr=OSD_ATTR_NUM_CUR_CMD_PARTITION_ID;
        
        prepare_get_attr_list_add_entry( req,
                                         page,
                                         attr,
                                         8);
    }

    {
	sec_cmd_params_t cmd_params;
	
	sec_cmd_params_init(&cmd_params,OSD_OBJ_TYPE_PART,
				*part_id,0,
				OSD_ATTRMASK_SOMEPAGE, //for current command page
				OSD_ATTRMASK_NONE);
				
				
	wait_req(req, 5, 1, 0, OSD_CMD_CREATE_PARTITION, &cmd_params);
    }
    
    osd_get_result(req, &res);
    
    if(OSD_RC_IS_GOOD(res.rc)) {
        if(*part_id == 0) {
            if (extract_attribute_from_list(req, 
                                            page, 
                                            attr,
                                            sizeof(uint64_t),
                                            (void *)part_id) == -1) {
                OSD_OPS_ERROR("extract_attributes() failed\n");
                free_osd_req(req);
                return -1;
            };
            
            *part_id = NTOHLL(*part_id);
        }
    }
    
    PRINT("finished osd create partition id 0x%llx result %s\n",*part_id , osd_rc_name(res.rc));
    
    free_osd_req(req);
    
    if(OSD_RC_IS_GOOD(res.rc))
        return 0;
    else
        return -1;
}

int osd_create_object(osd_dev_handle_t dev, uint64_t part_id, uint64_t * oid)
{
    struct osd_req_t * req;
    uint32_t page,attr;
    osd_result_t res;
    
    req = prepare_osd_create(dev, part_id, *oid);
    
    if(req == NULL ) {
        OSD_OPS_ERROR("failed to create partition\n");
        return -1;
    }
    
    page=OSD_PAGE_NUM_CUR_CMD; 
    attr=OSD_ATTR_NUM_CUR_CMD_OBJ_ID;
 
    prepare_get_attr_list_add_entry( req,
                                     page,
                                     attr,
                                     8);

    {
        sec_cmd_params_t cmd_params;
	
	sec_cmd_params_init(&cmd_params,OSD_OBJ_TYPE_UOBJ,
				part_id,*oid,
				OSD_ATTRMASK_SOMEPAGE, //for current command page
				OSD_ATTRMASK_NONE);
				
				
	wait_req(req, 5, 1, 0, OSD_CMD_CREATE, &cmd_params);
    }
     
    osd_get_result(req, &res);
    if(OSD_RC_IS_GOOD(res.rc)) {
        /* we got a completion from SCSI */
        if (extract_attribute_from_list(req,
                                        page, 
                                        attr,
                                        sizeof(uint64_t),
                                        (void *)oid) == -1) {
            OSD_OPS_ERROR("extract_attributes() failed\n");
            free_osd_req(req);
            return -1;
        };
    
        *oid = NTOHLL(*oid);
    }    
    
    PRINT("finished osd create oid = 0x%Lx req result = %s\n", *oid, osd_rc_name(res.rc));
    
    free_osd_req(req);
    
    if(OSD_RC_IS_GOOD(res.rc)) {
        return 0;
    }
    else {
        return -1;
    }
}

int osd_format(osd_dev_handle_t dev, uint64_t tot_capacity)
{
    struct osd_req_t * req;
    osd_result_t res;
    int rc;
    
    req = prepare_osd_format_lun(dev,tot_capacity);
    
    if(req == NULL ) {
        OSD_OPS_ERROR("failed to format\n");
        return -1;
    }
    
    {
	sec_cmd_params_t cmd_params;
	
	sec_cmd_params_init(&cmd_params,OSD_OBJ_TYPE_ROOT,
				0,0,
				OSD_ATTRMASK_NONE, OSD_ATTRMASK_NONE);
				
				
	wait_req(req, 5*60, 1, 0, OSD_CMD_FORMAT, &cmd_params);
    }
     
    PRINT("finished osd format\n");
    
    osd_get_result(req, &res);
    if (OSD_RC_IS_GOOD(res.rc)) {
	rc = 0;
    } else {
        rc = -1;
    }
    
    free_osd_req(req);
    
    return rc;
}

int osd_write(osd_dev_handle_t dev, 
              uint32_t part_id, 
              uint64_t oid, 
              const char * data_out, 
              uint64_t offset, 
              uint64_t len, 
              uint64_t * obj_len)
{
    struct osd_req_t * req;
    uint32_t page_num, attr_num;
    uint64_t obj_len_val;
    osd_result_t res;
    int rc;
    
    req = prepare_osd_write(dev, 
                            part_id, 
                            oid, 
                            len,
                            offset,
                            0,
                            data_out);
    
    if(req == NULL ) {
        OSD_OPS_ERROR("failed to write t object\n");
        return -1;
    }
    
    page_num=OSD_PAGE_NUM_UOBJ_INFO;
    attr_num=OSD_ATTR_NUM_UOBJ_INFO_OBJ_LENGTH;;
    
    PRINT("get attribute page 0x%x num 0x%x\n", page_num, attr_num);
    
    prepare_get_attr_list_add_entry( req,
                                     page_num,
                                     attr_num,
                                     8);
    
    
    
    {
	sec_cmd_params_t cmd_params;
	
	sec_cmd_params_init(&cmd_params,OSD_OBJ_TYPE_UOBJ,
				part_id,oid,
				OSD_ATTRMASK_NORMAL, OSD_ATTRMASK_NONE);
				
				
	wait_req(req, 5, 1, 0, OSD_CMD_WRITE, &cmd_params);
    }
     
    osd_get_result(req, &res);
    if(OSD_RC_IS_GOOD(res.rc)) {
        rc = 0;
        if (extract_attribute_from_list(req,
                                        page_num, 
                                        attr_num,
                                        sizeof(uint64_t),
                                        (void *)&obj_len_val) == -1) {
            OSD_OPS_ERROR("Cant extract attributes from list\n");
        }
        else {
            *obj_len=NTOHLL(obj_len_val);
            PRINT("page %d attr %d val 0x%llx\n", 
                  page_num, attr_num, *obj_len);
        }
    }
    else {
        rc = -1;
    }
    
    PRINT("finished osd write req result %s\n", osd_rc_name(res.rc));
    
    free_osd_req(req);

    return rc;
}


int osd_read(osd_dev_handle_t dev, 
             uint64_t part_id, 
             uint64_t oid,
             uint64_t offset,
             char * data_in, 
             uint64_t len)
{
    struct osd_req_t * req;
    
    req = prepare_osd_read(dev, 
                           part_id, 
                           oid, 
                           len,
                           offset,
                           0,
                           data_in);
                          
    
    if(req == NULL ) {
        OSD_OPS_ERROR("failed to read \n");
        return -1;
    }
    
     {
	sec_cmd_params_t cmd_params;
	
	sec_cmd_params_init(&cmd_params,OSD_OBJ_TYPE_UOBJ,
				part_id,oid,
				OSD_ATTRMASK_NONE, OSD_ATTRMASK_NONE);
				
				
	wait_req(req, 5, 1, 0, OSD_CMD_READ, &cmd_params);
    }
     
    free_osd_req(req);
    
    return 0;
}

int osd_clear(osd_dev_handle_t dev, 
              uint32_t part_id, 
              uint64_t oid, 
              uint64_t offset, 
              uint64_t len)
{
    struct osd_req_t * req;
    osd_result_t res;
    int rc;
    
    req = prepare_osd_clear(dev, 
                            part_id, 
                            oid, 
                            len,
                            offset);
    
    if(req == NULL ) {
        OSD_OPS_ERROR("failed to clear object\n");
        return -1;
    }
    
    {
	sec_cmd_params_t cmd_params;
	
	sec_cmd_params_init(&cmd_params,OSD_OBJ_TYPE_UOBJ,
			    part_id,oid,
			    OSD_ATTRMASK_NORMAL, OSD_ATTRMASK_NONE);
	
	
	//for clear we wait 100 seconds 
	wait_req(req, 100, 1, 0, OSD_CMD_CLEAR, &cmd_params);
    }
     
    osd_get_result(req, &res);
    if(OSD_RC_IS_GOOD(res.rc)) {
	rc = 0;
    }
    else {
        rc = -1;
    }
    
    PRINT("finished osd clear req result %s\n", osd_rc_name(res.rc));
    
    free_osd_req(req);

    return rc;
}

int osd_list(osd_dev_handle_t dev, 
             uint64_t part_id, 
             char * data_in,
             uint32_t *tot_entries,
             uint64_t *list_arr[],
             uint64_t len)
{
    struct osd_req_t * req;
    uint64_t cont_tag,total_entries,current_entries;
    uint32_t list_id;
    osd_result_t res;
    int is_partitions;
    int is_not_uptodate;
    sec_cmd_params_t cmd_params;
	    
    sec_cmd_params_init(&cmd_params,
			    (part_id==0)?OSD_OBJ_TYPE_ROOT:OSD_OBJ_TYPE_PART,
			    part_id,0,
			    OSD_ATTRMASK_NONE, OSD_ATTRMASK_NONE);

    list_id=0;
    cont_tag=0;
    *tot_entries=0;

    do {
	req = prepare_osd_list(dev, 
			       part_id, 
			       list_id,
			       len,
			       cont_tag,
			       0/*no sg usage*/,
			       data_in);
                          
	
	if(req == NULL ) {
	    OSD_OPS_ERROR("failed to prepare list \n");
	    return -1;
	}
	
	wait_req(req, 5, 1, 0, OSD_CMD_LIST, &cmd_params);
	
        osd_get_result(req, &res);

	PRINT("finished osd list req result = %s\n", osd_rc_name(res.rc));
        
	if (OSD_RC_IS_GOOD(res.rc)) {
            
            if (extract_list_from_req(req,
				      &total_entries,
				      &current_entries,
				      list_arr,
				      &is_partitions,
				      &is_not_uptodate,
				      &cont_tag,
				      &list_id)) {
		free_osd_req(req);
		return 1;
	    }
            *tot_entries += current_entries;
            PRINT("\n");
            
	    free_osd_req(req);
	    
	} else { //command returned with nonzero status code
	    free_osd_req(req);
	    return 1;
	}
    } while (cont_tag != 0); //send another req if we got nonzero cont-tag
	
    return 0;
}
    
int osd_remove(osd_dev_handle_t dev, uint64_t part_id, uint64_t oid)
{
    struct osd_req_t * req;
    osd_result_t res;
    
    req = prepare_osd_remove(dev, 
                           part_id, 
                             oid);
    
    if(req == NULL ) {
        OSD_OPS_ERROR("failed to read \n");
        return -1;
    }
    
    {
	sec_cmd_params_t cmd_params;
	
	sec_cmd_params_init(&cmd_params,OSD_OBJ_TYPE_UOBJ,
				part_id,oid,
				OSD_ATTRMASK_NONE, OSD_ATTRMASK_NONE);
				
				
	wait_req(req, 5, 1, 0, OSD_CMD_REMOVE, &cmd_params);
    }
     
    osd_get_result(req, &res);
    PRINT("finished osd remove req result = %s\n", osd_rc_name(res.rc));
    
    free_osd_req(req);
    
    if(OSD_RC_IS_GOOD(res.rc)) {
        return 0;
    }
    else {
        return -1;
    }
}

int osd_remove_partition(osd_dev_handle_t dev, uint64_t part_id)
{
    struct osd_req_t * req;
    osd_result_t res;
    
    req = prepare_osd_remove_partition(dev, part_id);
    
    if(req == NULL ) {
        OSD_OPS_ERROR("failed to prepare req \n");
        return -1;
    }
    
    {
	sec_cmd_params_t cmd_params;
	
	sec_cmd_params_init(&cmd_params,OSD_OBJ_TYPE_PART,
				part_id,0,
				OSD_ATTRMASK_NONE, OSD_ATTRMASK_NONE);
				
				
	wait_req(req, 5, 1, 0, OSD_CMD_REMOVE_PARTITION, &cmd_params);
    }
    
    osd_get_result(req, &res);
    PRINT("finished osd remove partition req result = %s\n", osd_rc_name(res.rc));
        
    free_osd_req(req);
    
    if(OSD_RC_IS_GOOD(res.rc)) {
        return 0;
    }
    else {
        return -1;
    }
}

int osd_set_one_attr(osd_dev_handle_t dev, 
                     uint64_t part_id, 
                     uint64_t oid, 
                     uint32_t page, 
                     uint32_t attr, 
                     uint32_t len, 
                     void * value)
{
    struct osd_req_t * req;
    
    req = prepare_osd_set_attr(dev, part_id, oid);
    
    if(req == NULL) {
        OSD_OPS_ERROR("failed to create partition\n");
        return -1;
    }

    prepare_set_attr_list_add_entry( req,
                                     page,
                                     attr,
                                     len,
                                     value);

    {
	sec_cmd_params_t cmd_params;
	sec_obj_type_t t;

	if (oid) {
	    t=OSD_OBJ_TYPE_UOBJ;
	} else if (part_id) {
	    t=OSD_OBJ_TYPE_PART;
	} else {
	    t=OSD_OBJ_TYPE_ROOT;
	}
	
	sec_cmd_params_init(&cmd_params,
				t,
				part_id,oid,
				OSD_ATTRMASK_NONE, OSD_ATTRMASK_NORMAL);
				
				
	wait_req(req, 5, 1, 0, OSD_CMD_SET_ATTR, &cmd_params);
    }

    free_osd_req(req);
    
    return 0;
}

int osd_get_one_attr(osd_dev_handle_t dev, uint64_t part_id, uint64_t oid, uint32_t page, uint32_t attr, uint32_t alloc_len, uint16_t * len, void * value)
{
    struct osd_req_t * req;
    void * buffer;
    char attr0[40];
        
    buffer = os_malloc(alloc_len + 16);
    
    req = prepare_osd_get_attr(dev, part_id, oid);
        
    if(req == NULL) {
        OSD_OPS_ERROR("failed to create partition\n");
        return -1;
    }
    
    prepare_get_attr_list_add_entry( req,
                                     page,
                                     attr,
                                     alloc_len + 16); /* FIXME */

    if (page<0x10) {
      prepare_get_attr_list_add_entry( req,
				       page,
				       0,
				       alloc_len + 16); /* FIXME */
    }
    
    {
	sec_cmd_params_t cmd_params;
	sec_obj_type_t t;

	if (oid) {
	    t=OSD_OBJ_TYPE_UOBJ;
	} else if (part_id) {
	    t=OSD_OBJ_TYPE_PART;
	} else {
	    t=OSD_OBJ_TYPE_ROOT;
	}
	
	sec_cmd_params_init(&cmd_params,
			    t,
			    part_id,oid,
			    OSD_ATTRMASK_NORMAL, OSD_ATTRMASK_NONE);
				
				
	wait_req(req, 5, 1, 0, OSD_CMD_SET_ATTR, &cmd_params);
    }
    
    if (page<0x10) {
      if (extract_attribute_from_list(req, 
                                      page, 
                                      0,
				      40,
                                      (void*)attr0) == -1) {
        OSD_OPS_ERROR("extract_attributes() of attr 0 failed\n");
      } else {
	attr0[40]='\0';
	PRINT("got page id : %s\n",attr0);
      }
    }
    
    if (extract_attribute_from_list(req,
                                    page, 
                                    attr,
				    alloc_len,
                                    value) == -1) {
        OSD_OPS_ERROR("extract_attributes() failed\n");
        free_osd_req(req);
        return -1;
    };
    
    free_osd_req(req);
    
    os_free(buffer);
    
    return 0;
}

int osd_inquiry_83(osd_dev_handle_t dev, char * data_in, uint64_t len)
{
    struct osd_req_t * req;
    osd_result_t res;
    
    req = prepare_osd_inquiry(dev, len, data_in, 0x83);
    if(req == NULL ) {
        OSD_OPS_ERROR("failed to inquiry 0x83 \n");
        return -1;
    }
    
    wait_req(req, 5, 1, 1, OSD_CMD_INQUIRY_83, NULL);
    osd_get_result(req, &res);

    free_osd_req(req);
    
    if (OSD_RC_IS_GOOD(res.rc)) {    
        return 0;
    }
    return -1;
}

//this command is the only one using CAPKEY
int osd_set_root_key(osd_dev_handle_t dev, 
		     uint8_t seed_a[OSD_CRYPTO_VAL_SIZE],
		     uint8_t new_key_id_a[7])
{
    struct osd_req_t * req;
    osd_result_t res;
    uint8_t credential_a[OSD_CREDENTIAL_SIZE];
    uint8_t key_a[OSD_CRYPTO_VAL_SIZE],audit_a[OSD_CRYPTO_VAL_SIZE];
    uint8_t sys_id_a[20];
    osd_channel_token_t ch_token;
    array12_t discriminator;
    sec_cmd_params_t cmd_params;
    uint16_t service_action = OSD_CMD_SET_KEY;
    int rc;
	
    req = prepare_osd_set_key(dev, 0x01/*root key*/,
			      0/*pid*/, 0/*key ver*/,
			      new_key_id_a,
			      seed_a);
    if(req == NULL ) {
        OSD_OPS_ERROR("failed to prepare set key req \n");
        return -1;
    }

    //prepare sec stuff:
    memcpy(key_a, SEC_INITIAL_MASTER_KEY, OSD_CRYPTO_VAL_SIZE);

    rc=get_osd_system_id(dev, sys_id_a);
    if (rc) return -1;
    rc=get_osd_channel_token(dev, &ch_token);
    if (rc) return -1;

    sec_cmd_params_init(&cmd_params,OSD_OBJ_TYPE_ROOT,
			0,0,
			OSD_ATTRMASK_NONE, OSD_ATTRMASK_NONE);
    
    memset(audit_a,0,sizeof(audit_a));
    memset(discriminator.b,0,sizeof(discriminator.b));
        
    sec_generate_credential(credential_a,
			    1, //1 service actions
			    &service_action, //service actions array
			    &cmd_params,
			    OSD_CAPKEY, //security method to use
			    sys_id_a, //system id of target
			    0, //key version
			    key_a, //key to use
			    (uint64_t)0x100000000000LL, //expiration time
			    audit_a, //audit
			    discriminator,
			    0, //create time - 0=don't check
			    0); //policy acces tag, 0=don't check

    //send req
    wait_osd_req(req,5/*timeout*/,1/*retries*/, credential_a, &ch_token);

    osd_get_result(req, &res);

    free_osd_req(req);

    PRINT("finished osd set root key req. result = %s\n", osd_rc_name(res.rc));
    
    if (OSD_RC_IS_GOOD(res.rc)) {    
        return 0;
    }
    return -1;
}


