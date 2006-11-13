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
#include "osd_req.h"
#include "osd_req_imp.h"
#include "sec_initiator.h"

//allocation length made by initiator for vpd pages
#define VPD_PAGE_MAX_LEN 0xff

#if ( (OSD_CHANNEL_TOKEN_MAX_LEN+4) > VPD_PAGE_MAX_LEN )
#error "OSD_CHANNEL_TOKEN_MAX_LEN is too big to fit into VPD_PAGE_MAX_LEN"
#endif

#if 0  /* now unused  --pw */

/* common buffer for allignment needs */ 
static char pad[255] = {0};

static int build_data_in(osd_req_t * req)
{
    int offset = 0, iov_count = 0, tmp;
    struct iovec * iov;
    
    /* we allocate cmd_data_use_sg + 3:
       DATA IN
       256 byte allignment
       Get attributes
    */
    iov = (struct iovec*) os_malloc((req->cmd_data_use_sg + 3) * sizeof(struct iovec));
    if(iov == NULL) {
        OSD_ERROR("Failed to allocate iovec\n");
        return -ENOMEM;
    }
#ifdef __KERNEL__
    printk(KERN_INFO "%s: alloced %d iov at %p\n", __func__, req->cmd_data_use_sg + 3, iov);
#endif
    req->allocated_data_in = 1;
    
    if(req->cmd_data_direction == DMA_FROM_DEVICE) {
        /* we allow reads and writes with 0 length */
        if(req->cmd_data_len > 0) {
            offset += req->cmd_data_len;
            if(req->cmd_data_use_sg > 0) {
                
                memcpy(iov, req->cmd_data, (req->cmd_data_use_sg * sizeof(struct iovec)));
                iov_count += req->cmd_data_use_sg;
            }
            else {
                iov[0].iov_base = req->cmd_data;
                iov[0].iov_len = req->cmd_data_len;
                
                iov_count++;
            }
        }
    }
#ifdef __KERNEL__
    printk(KERN_INFO "%s: iov[0] %p len %lu count %d\n", __func__,
           iov[0].iov_base, iov[0].iov_len, iov_count);
#endif
    
    if(req->retrieved_attr_len > 0) {
        if(offset & (255)) {
            /* set attr padding */
            tmp = offset;
            offset += 255;
            offset &= ~(255);
            
            iov[iov_count].iov_base = &pad;
            iov[iov_count].iov_len = offset - tmp;
            iov_count++;
        }
        
        req->retrieved_attr_offset = offset;
        offset += req->retrieved_attr_len;
        
        req->retrieved_attr_buf = (unsigned char *) os_malloc(req->retrieved_attr_len);
        if(req->retrieved_attr_buf == NULL) {
            OSD_ERROR("Failed to allocate retrieved attr buffer\n");
            return -ENOMEM;
        }
        req->allocated_retrieved_attr_buf = 1;
        
        iov[iov_count].iov_base = req->retrieved_attr_buf;
        iov[iov_count].iov_len = req->retrieved_attr_len;
        iov_count++;
    }

#ifdef __KERNEL__
    printk(KERN_INFO "%s: (end) iov[0] %p len %lu count %d data_in_len %d\n", __func__,
           iov[0].iov_base, iov[0].iov_len, iov_count, offset);
#endif
    
    req->scsi_desc.data_in = (unsigned char*)iov;
    req->scsi_desc.data_in_len = offset;
    req->scsi_desc.use_sg_in = iov_count;
    
    return 0;
}

static int build_data_out(osd_req_t * req)
{
    int offset = 0, iov_count = 0, tmp;
    struct iovec * iov;
    
    /* we allocate cmd_data_use_sg + 5:
       DATA IN
       256 byte allignment
       Set attributes
       256 byte allignment
       Get attributes
    */

#ifdef __KERNEL__
    if (req->cmd_data_use_sg + 5 > 64) return -ENOMEM;
    iov = req->data_out_iov;
#else  
    iov = (struct iovec*) os_malloc((req->cmd_data_use_sg + 5) * sizeof(struct iovec));
    if(iov == NULL) {
        OSD_ERROR("Failed to allocate iovec\n");
        return -ENOMEM;
    }
    req->allocated_data_out = 1;
#endif

    if(req->cmd_data_direction == DMA_TO_DEVICE) {
        if(req->cmd_data_len > 0) {
            offset += req->cmd_data_len;
            
            if(req->cmd_data_use_sg > 0) {
                
                memcpy(iov, req->cmd_data, (req->cmd_data_use_sg * sizeof(struct iovec)));
                iov_count += req->cmd_data_use_sg;
            }
            else {
                iov[0].iov_base = req->cmd_data;
                iov[0].iov_len = req->cmd_data_len;
                
                iov_count++;
            }
        }
    }
    
    if(req->new_set_list_len > 0) {
        if(offset & (255)) {
            /* set attr padding */
            tmp = offset;
            offset += 255;
            offset &= ~(255);
            
            iov[iov_count].iov_base = &pad;
            iov[iov_count].iov_len = offset - tmp;
            iov_count++;
        }
        
        req->set_attribute_list_offset = offset;
        offset += req->new_set_list_len;
        iov[iov_count].iov_base = req->new_set_list_buf;
        iov[iov_count].iov_len = req->new_set_list_len;
        iov_count++;
    }

    if(req->new_get_list_len > 0) {
        if(offset & (255)) {
            /* get attr padding */
            tmp = offset;
            offset += 255;
            offset &= ~(255);
            
            iov[iov_count].iov_base = &pad;
            iov[iov_count].iov_len = offset - tmp;
            iov_count++;
        }
        
        req->get_attribute_list_offset = offset;
        offset += req->new_get_list_len;
        
        iov[iov_count].iov_base = req->new_get_list_buf;
        iov[iov_count].iov_len = req->new_get_list_len;
        iov_count++;
    }
    
    req->scsi_desc.data_out = (unsigned char*)iov;
    req->scsi_desc.data_out_len = offset;
    req->scsi_desc.use_sg_out = iov_count;
    
    return 0;
}
#endif  /* unused funcs  --pw */

static int prepare_scsi_param(osd_req_t * req,
                              unsigned int timeout, 
                              uint8_t credential[OSD_CREDENTIAL_SIZE],
			      osd_channel_token_t *channel_token)
{
    int cmd_data_direction = DMA_BIDIRECTIONAL;
    int ret;
        
    osd_assert(req);

    if (! req->non_osd_cmd) {
        osd_assert(credential);
    }
    
    if( timeout != 0) {
        req->scsi_desc.timeout = timeout;
    }
    else {
        req->scsi_desc.timeout = OSD_DEFAULT_TIMEOUT;
    }
    
    req->scsi_desc.max_retries = 1; // no retries 
    
    memset (req->scsi_desc.sense_buf, 0, sizeof(req->scsi_desc.sense_buf)); 
    req->scsi_desc.sense_ptr = req->scsi_desc.sense_buf;

    if (! req->non_osd_cmd) {
        osd_encap_cdb_hdr(req->scsi_desc.cdb,
                          0x7F,
                          0,
                          OSD_CDB_LEN,
                          req->service_action,req->timestamp_ctl);
    }
    
    switch(req->service_action) {

    case OSD_CMD_FORMAT:
	cmd_data_direction = DMA_NONE;
	osd_encap_cdb_cmd_format(req->scsi_desc.cdb,
				 req->cmd_params.format.formatted_capacity);
	break;

    case OSD_CMD_SET_KEY:
	cmd_data_direction = DMA_BIDIRECTIONAL;
	osd_encap_cdb_cmd_set_key(
	    req->scsi_desc.cdb,
	    req->cmd_params.set_key.key_to_set,
	    req->cmd_params.set_key.part_id,
	    req->cmd_params.set_key.key_ver,
	    req->cmd_params.set_key.key_id_a,
	    req->cmd_params.set_key.seed_a);
	break;
	    
    case OSD_CMD_READ:
	cmd_data_direction = DMA_FROM_DEVICE;
            
        osd_encap_cdb_cmd_read( req->scsi_desc.cdb,
                                req->cmd_params.read.part_id,
                                req->cmd_params.read.obj_id,
                                req->cmd_params.read.length,
                                req->cmd_params.read.offset);
        
        
            
        break;

    case OSD_CMD_INQUIRY:
#ifdef __KERNEL__
	printk(KERN_INFO "%s: inquiry: OSD_CDB_LEN %lu, read length %llu\n", __func__,  OSD_CDB_LEN, req->cmd_params.read.length);
#endif
        cmd_data_direction = DMA_FROM_DEVICE;

        osd_encap_cdb_cmd_inquiry(req->scsi_desc.cdb,
                                  req->cmd_params.read.length,
                                  0);

        break;

    case OSD_CMD_INQUIRY_83:
#ifdef __KERNEL__
	printk(KERN_INFO "%s: inquiry83: OSD_CDB_LEN %lu, read length %llu, cmd_data %p len %d use_sg %d\n", __func__,  OSD_CDB_LEN, req->cmd_params.read.length, req->cmd_data, req->cmd_data_len, req->cmd_data_use_sg);
#endif
        cmd_data_direction = DMA_FROM_DEVICE;

        osd_encap_cdb_cmd_inquiry(req->scsi_desc.cdb,
                                  req->cmd_params.read.length,
                                  0x83);

        break;

    case OSD_CMD_INQUIRY_B0:
        cmd_data_direction = DMA_FROM_DEVICE;

        osd_encap_cdb_cmd_inquiry(req->scsi_desc.cdb,
                                  req->cmd_params.read.length,
                                  0xB0);

        break;

    case OSD_CMD_INQUIRY_B1:
        cmd_data_direction = DMA_FROM_DEVICE;

        osd_encap_cdb_cmd_inquiry(req->scsi_desc.cdb,
                                  req->cmd_params.read.length,
                                  0xB1);

        break;
        
    case OSD_CMD_LIST:
    {
        cmd_data_direction = DMA_FROM_DEVICE;
            
        osd_encap_cdb_cmd_list( req->scsi_desc.cdb,
                                req->cmd_params.list.part_id,
				req->cmd_params.list.list_id,
                                req->cmd_params.list.alloc_len,
                                req->cmd_params.list.initial_obj_id);
    }   
            
        break;

    case OSD_CMD_WRITE:
        cmd_data_direction = DMA_TO_DEVICE;
            
        osd_encap_cdb_cmd_write( req->scsi_desc.cdb,
                                 req->cmd_params.write.part_id,
                                 req->cmd_params.write.obj_id,
                                 req->cmd_params.write.length,
                                 req->cmd_params.write.offset);
            
        break;
            
    case OSD_CMD_DEBUG:
        cmd_data_direction = DMA_TO_DEVICE;
            
        osd_encap_cdb_cmd_debug( req->scsi_desc.cdb,
                                 req->cmd_params.debug.data_out_len,
                                 req->cmd_params.debug.data_in_len);
            
        break;
            
    case OSD_CMD_CLEAR:
        cmd_data_direction = DMA_BIDIRECTIONAL ;
        osd_encap_cdb_cmd_write( req->scsi_desc.cdb,
                                 req->cmd_params.clear.part_id,
                                 req->cmd_params.clear.obj_id,
                                 req->cmd_params.clear.length,
                                 req->cmd_params.clear.offset);
            
        break;
            
    case OSD_CMD_SET_ATTR:
    case OSD_CMD_GET_ATTR:
        cmd_data_direction = DMA_BIDIRECTIONAL;
        
        if(req->service_action == OSD_CMD_SET_ATTR) {
            
            osd_encap_cdb_cmd_getset_attr(req->scsi_desc.cdb, 
                                          req->cmd_params.set_attr.part_id,
                                          req->cmd_params.set_attr.obj_id);
            
        }
        else {
            osd_encap_cdb_cmd_getset_attr(req->scsi_desc.cdb, 
                                          req->cmd_params.get_attr.part_id,
                                          req->cmd_params.get_attr.obj_id);
            
        }
        break;
            
    case OSD_CMD_CREATE_PARTITION:
        cmd_data_direction = DMA_BIDIRECTIONAL;
        
        osd_encap_cdb_cmd_create_partition(req->scsi_desc.cdb, 
                                           req->cmd_params.create_partition.requested_id);
            
        break;
        
    case OSD_CMD_CREATE:
        cmd_data_direction = DMA_BIDIRECTIONAL;
            
        osd_encap_cdb_cmd_create(req->scsi_desc.cdb, 
                                 req->cmd_params.create.part_id,
                                 req->cmd_params.create.requested_id,
                                 req->cmd_params.create.num_objects);
            
        break;
            
    case OSD_CMD_REMOVE:
        cmd_data_direction = DMA_BIDIRECTIONAL;
            
        osd_encap_cdb_cmd_remove(req->scsi_desc.cdb, 
                                 req->cmd_params.remove.part_id,
                                 req->cmd_params.remove.obj_id);
            
        break;
        
    case OSD_CMD_REMOVE_PARTITION:
            
        cmd_data_direction = DMA_BIDIRECTIONAL;
            
        osd_encap_cdb_cmd_remove_partition(req->scsi_desc.cdb, 
                                           req->cmd_params.remove_partition.part_id);
            
        break;
            
    default:
        OSD_TRACE(OSD_TRACE_REQ,1,
		  "unsupported OSD service action 0x%x\n", req->service_action);
        return -EINVAL;
            
    }
    req->cmd_data_direction = cmd_data_direction;

    /* handle set get attributre */
    if(req->get_set_attr_fmt == OSD_GET_SET_LIST_FMT) {
        
        /* init set get attribute list length */
        if(req->new_set_list_len > 0 &&
           osd_attr_get_list_entries_length(&req->set_list_desc) > 0) {
            req->new_set_list_len = osd_attr_get_list_buf_length(&req->set_list_desc);
            osd_assert(req->new_set_list_len > 0); /* verify we dont have a bug */
        }
        if(req->new_get_list_len > 0 &&
           osd_attr_get_list_entries_length(&req->get_list_desc) > 0) {
            req->new_get_list_len = osd_attr_get_list_buf_length(&req->get_list_desc);
            osd_assert(req->new_get_list_len > 0);
        }
    }
    else if(req->get_set_attr_fmt == OSD_GET_SET_PAGE_FMT) {
            
	OSD_TRACE(OSD_TRACE_REQ,1,
		  "unsupported OSD service action 0x%x\n", req->service_action);
        return -EINVAL;
    }

#if 0  /* --pw */
    /* build DATA_OUT buffer and offsets */
    if((ret = build_data_out(req))) {
        OSD_ERROR("Failed to build data_out buffer\n");
        return ret;
    }
    
    /* build DATA_IN buffers and offsets*/
    if((ret = build_data_in(req))) {
        OSD_ERROR("Failed to build data_in buffer\n");
        return ret;
    }
#else
    ret = -EINVAL;
    if (req->cmd_data_use_sg != 0) {
	OSD_ERROR("Cannot use sg\n");
	return ret;
    }
    if (req->retrieved_attr_len != 0) {
	OSD_ERROR("Not sure what to do about retrieved data\n");
	return ret;
    }
    if (req->new_set_list_len != 0) {
	OSD_ERROR("Not sure what to do about new set list len\n");
	return ret;
    }
    if (req->new_get_list_len != 0) {
	OSD_ERROR("Not sure what to do about new get list len\n");
	return ret;
    }
    req->allocated_data_in = 0;
    req->scsi_desc.use_sg_in = 0;
    if (req->cmd_data_direction == DMA_FROM_DEVICE) {
	req->scsi_desc.data_in = req->cmd_data;
	req->scsi_desc.data_in_len = req->cmd_data_len;
    }
    req->allocated_data_out = 0;
    req->scsi_desc.use_sg_out = 0;
    if (req->cmd_data_direction == DMA_TO_DEVICE) {
	req->scsi_desc.data_out = req->cmd_data;
	req->scsi_desc.data_out_len = req->cmd_data_len;
    }
#endif  /* --pw */

#ifdef __KERNEL__
    printk(KERN_INFO "%s: data_in_len %d, data_out_len %d\n", __func__,
           req->scsi_desc.data_in_len,
           req->scsi_desc.data_out_len);
#endif
    
    if(req->get_set_attr_fmt == OSD_GET_SET_LIST_FMT) {
        
        /* encap set get attributes params in cdb */
        osd_encap_cdb_attr_list(req->scsi_desc.cdb,
                                req->new_get_list_len, 
                                req->get_attribute_list_offset,
                                req->retrieved_attr_len, 
                                req->retrieved_attr_offset,
                                req->new_set_list_len,
                                req->set_attribute_list_offset);
	
    }

    if (! req->non_osd_cmd) {
        //set security parts
        {
            array12_desc_t arr12;
	    
            memset(arr12.b,0,sizeof(arr12)); //we don't use nonce in capkey
            sec_set_cdb_sec_params((uint8_t*)req->scsi_desc.cdb,
				   OSD_CDB_LEN,
                                   (uint8_t*)credential,
				   channel_token,
                                   arr12,  //nonce
                                   0,0); //data int_chk_val offsets
        }
    }
    
    return 0;
}

static osd_req_t * prepare_osd_req( osd_dev_handle_t dev, 
                                    int cmd_data_use_sg,
                                    char * cmd_data,
                                    uint32_t cmd_data_len)
{
    osd_req_t * req;
        
    OSD_TRACE(OSD_TRACE_REQ,3, "ENTER\n");
    
    req = os_alloc_osd_req();
    if(req == NULL) {
        return req;
    }

    req->dev = dev;
    req->get_set_attr_fmt = OSD_GET_SET_INVALID_FMT;
    
    req->new_get_list_len = 0;
    req->new_set_list_len = 0;
    
    req->scsi_desc.cdb_len = MAX_COMMAND_SIZE;
    
    /* init cmd data desc */
    req->cmd_data_use_sg = cmd_data_use_sg;
    req->cmd_data_len = cmd_data_len;
    req->cmd_data = (unsigned char *) cmd_data;
#ifdef __KERNEL__
    printk(KERN_INFO "%s: using cmd_data %p len %d use_sg %d\n", __func__,
           req->cmd_data, req->cmd_data_len, req->cmd_data_use_sg);
#endif
    
    //by default we use default timestamps behaviour
    //req->timestamp_ctl=OSD_CDB_TIMESTAMP_NO_CTL;

    //req->sec.part_id=0;
    //req->sec.obj_id=0;
    //req->sec.req_id=0;

    osd_assert(req->cmd_data_use_sg == 0); //doesn't suppport sg for now
    
    OSD_TRACE(OSD_TRACE_REQ,3, "EXIT\n");
    return req;
}

osd_req_t * prepare_osd_format_lun(osd_dev_handle_t dev, 
				   uint64_t formatted_capacity)
{
    osd_req_t * req;
    
    req = prepare_osd_req(dev,
                          0, 
                          NULL, 
                          0);
    if(req != NULL) {
        req->service_action = OSD_CMD_FORMAT;
        req->cmd_params.format.formatted_capacity=formatted_capacity;

	//req->sec.obj_type=OSD_OBJ_TYPE_ROOT;
    }
    
    return req;
}
EXPORT_SYMBOL(prepare_osd_format_lun);

osd_req_t * prepare_osd_create_partition(osd_dev_handle_t dev, 
                                         uint64_t requested_id)
{
    osd_req_t * req;

    req = prepare_osd_req(dev,
                          0, 
                          NULL, 
                          0);
    if(req != NULL) {
        req->service_action = OSD_CMD_CREATE_PARTITION;
        req->cmd_params.create_partition.requested_id = requested_id;

	//req->sec.req_id=requested_id;
	//req->sec.obj_type=OSD_OBJ_TYPE_PART;
    }
    
    return req;
}
EXPORT_SYMBOL(prepare_osd_create_partition);

osd_req_t * prepare_osd_create( osd_dev_handle_t dev, 
                                uint64_t part_id,
                                uint64_t requested_id)
{
    osd_req_t * req;

    req = prepare_osd_req(dev,
                          0,
                          NULL,
                          0);
    if(req != NULL) {
        req->service_action = OSD_CMD_CREATE;
        req->cmd_params.create.part_id = part_id;
        req->cmd_params.create.requested_id = requested_id;
	req->cmd_params.create.num_objects=0;

	//req->sec.obj_type=OSD_OBJ_TYPE_UOBJ;
	//req->sec.part_id=part_id;
	//req->sec.req_id=requested_id;
    }

    return req;
}
EXPORT_SYMBOL(prepare_osd_create);

osd_req_t * prepare_osd_write(osd_dev_handle_t dev, 
                              uint64_t part_id,
                              uint64_t obj_id,
                              uint64_t length,
                              uint64_t offset,
                              int use_sg,
                              const char * data)
{
    osd_req_t * req;

    if ((length > 65536) || /* I/Os are limited to 64K */
        ((length > 0) && (data == NULL))) {
        PRINT("%s failed: illegal parameters\n", __FUNCTION__);
        return NULL;
    }
    
    req = prepare_osd_req(dev,
                          use_sg,
                          (char *) data,
                          length);
    
    if(req != NULL) {
        req->service_action = OSD_CMD_WRITE;
        req->cmd_params.write.part_id = part_id;
        req->cmd_params.write.obj_id = obj_id;
        req->cmd_params.write.length = length;
        req->cmd_params.write.offset = offset;

	//req->sec.obj_type=OSD_OBJ_TYPE_UOBJ;
	//req->sec.part_id=part_id;
	//req->sec.obj_id=obj_id;
    }

    return req;
}
EXPORT_SYMBOL(prepare_osd_write);

osd_req_t * prepare_osd_clear(osd_dev_handle_t dev, 
                              uint64_t part_id,
                              uint64_t obj_id,
                              uint64_t length,
                              uint64_t offset)
{
    osd_req_t * req;

    req = prepare_osd_req(dev,
                          0,
                          NULL,
                          0);
    
    if(req != NULL) {
        req->service_action = OSD_CMD_CLEAR;
        req->cmd_params.clear.part_id = part_id;
        req->cmd_params.clear.obj_id = obj_id;
        req->cmd_params.clear.length = length;
        req->cmd_params.clear.offset = offset;

	//req->sec.obj_type=OSD_OBJ_TYPE_UOBJ;
	//req->sec.part_id=part_id;
	//req->sec.obj_id=obj_id;
    }

    return req;
}
EXPORT_SYMBOL(prepare_osd_clear);

osd_req_t * prepare_osd_read(osd_dev_handle_t dev, 
                             uint64_t part_id,
                             uint64_t obj_id,
                             uint64_t length,
                             uint64_t offset,
                             int use_sg,
                             char * data)
{
    osd_req_t * req;

    if ((length > 65536) || /* I/Os are limited to 64K */
        ((length > 0) && (data == NULL))) {
        PRINT("%s failed: illegal parameters\n", __FUNCTION__);
        return NULL;
    }

    
    req = prepare_osd_req(dev,
                          use_sg,
                          data,
                          length);
    if(req != NULL) {
        req->service_action = OSD_CMD_READ;
        req->cmd_params.read.part_id = part_id;
        req->cmd_params.read.obj_id = obj_id;
        req->cmd_params.read.length = length;
        req->cmd_params.read.offset = offset;

	//req->sec.obj_type=OSD_OBJ_TYPE_UOBJ;
	//req->sec.part_id=part_id;
	//req->sec.obj_id=obj_id;
    }

    return req;
}
EXPORT_SYMBOL(prepare_osd_read);

osd_req_t * prepare_osd_inquiry(osd_dev_handle_t dev, 
                                uint64_t length,
                                char * data,
                                uint8_t page)
{
    osd_req_t * req;

    if ((length < 16) || (data == NULL)) {
        PRINT("%s failed: illegal parameters\n", __FUNCTION__);
        return NULL;
    }
    
    req = prepare_osd_req(dev, 0, data, length);
    if(req != NULL) {
        req->non_osd_cmd = 1;
        req->service_action = OSD_CMD_INQUIRY | page;
        req->cmd_params.read.length = length;
    }

    return req;
}
EXPORT_SYMBOL(prepare_osd_inquiry);

osd_req_t * prepare_osd_list(osd_dev_handle_t dev, 
                             uint64_t part_id,
			     uint32_t list_id,
			     uint64_t alloc_len,
                             uint64_t initial_obj_id,
                             int use_sg,
                             char * data)
{
    osd_req_t * req;

    if ((alloc_len > 0) && (data == NULL)) {
        PRINT("%s failed: illegal parameters\n", __FUNCTION__);
        return NULL;
    }
        
    
    req = prepare_osd_req(dev,
                          use_sg,
                          data,
                          alloc_len);
    if(req != NULL) {
        req->service_action = OSD_CMD_LIST;
        req->cmd_params.list.part_id = part_id;
        req->cmd_params.list.list_id = list_id;
        req->cmd_params.list.alloc_len = alloc_len;
        req->cmd_params.list.initial_obj_id = initial_obj_id;

/* 	if (0==part_id) { */
/* 	    req->sec.obj_type=OSD_OBJ_TYPE_ROOT; */
/* 	} else { */
/* 	    req->sec.obj_type=OSD_OBJ_TYPE_PART; */
/* 	} */
/* 	req->sec.part_id=part_id; */
    }

    return req;
}
EXPORT_SYMBOL(prepare_osd_list);

osd_req_t * prepare_osd_remove( osd_dev_handle_t dev, 
                                uint64_t part_id,
                                uint64_t obj_id)
{
    osd_req_t * req;
    
    req = prepare_osd_req(dev,
                          0,
                          NULL,
                          0);
    if(req != NULL) {
        req->service_action = OSD_CMD_REMOVE;
        req->cmd_params.remove.part_id = part_id;
        req->cmd_params.remove.obj_id = obj_id;

/* 	req->sec.obj_type=OSD_OBJ_TYPE_UOBJ; */
/* 	req->sec.part_id=part_id; */
/* 	req->sec.obj_id=obj_id; */
    }

    return req;
}
EXPORT_SYMBOL(prepare_osd_remove);

osd_req_t * prepare_osd_remove_partition( osd_dev_handle_t dev, 
                                          uint64_t part_id)
{
    osd_req_t * req;

    req = prepare_osd_req(dev,
                          0,
                          NULL,
                          0);
    if(req != NULL) {
        req->service_action = OSD_CMD_REMOVE_PARTITION;
        req->cmd_params.remove_partition.part_id = part_id;

/* 	req->sec.obj_type=OSD_OBJ_TYPE_PART; */
/* 	req->sec.part_id=part_id; */
    }
    
    return req;
}
EXPORT_SYMBOL(prepare_osd_remove_partition);

/* static void sec_by_ids(osd_req_t * req,uint64_t part_id, uint64_t obj_id) */
/* { */
/*     req->sec.part_id=part_id; */
/*     req->sec.obj_id=obj_id; */
/*     if (0==part_id) { */
/* 	req->sec.obj_type=OSD_OBJ_TYPE_ROOT; */
/*     } else if (0==obj_id) { */
/* 	req->sec.obj_type=OSD_OBJ_TYPE_PART; */
/*     } else { */
/* 	req->sec.obj_type=OSD_OBJ_TYPE_UOBJ; */
/*     } */
/* } */


osd_req_t * prepare_osd_set_attr(osd_dev_handle_t dev, 
                                 uint64_t part_id,
                                 uint64_t obj_id)
{
    osd_req_t * req;

    req = prepare_osd_req(dev,
                          0,
                          NULL,
                          0);
    if(req != NULL) {
        req->service_action = OSD_CMD_SET_ATTR;
        req->cmd_params.remove.part_id = part_id;
        req->cmd_params.remove.obj_id = obj_id;

/* 	sec_by_ids(req,part_id,obj_id); */
    }
    
    return req;
}
EXPORT_SYMBOL(prepare_osd_set_attr);

osd_req_t * prepare_osd_get_attr(osd_dev_handle_t dev, 
                                 uint64_t part_id,
                                 uint64_t obj_id)
{
    osd_req_t * req;

    req = prepare_osd_req(dev,
                          0,
                          NULL,
                          0);
    if(req != NULL) {
        req->service_action = OSD_CMD_GET_ATTR;
        req->cmd_params.remove.part_id = part_id;
        req->cmd_params.remove.obj_id = obj_id;

/* 	sec_by_ids(req,part_id,obj_id); */
    }
    
    return req;
}
EXPORT_SYMBOL(prepare_osd_get_attr);

#define OSD_KEY_ID 7
#define OSD_SEED 20
osd_req_t * prepare_osd_set_key(osd_dev_handle_t dev, 
				uint8_t key_to_set,
				uint64_t part_id,
				uint8_t key_ver,
				uint8_t key_id_a[OSD_KEY_ID],
				uint8_t seed_a[OSD_SEED])
{
    osd_req_t * req;

    if ((NULL == key_id_a) || (NULL == seed_a)) {
        PRINT("%s failed: illegal parameters\n", __FUNCTION__);
        return NULL;
    }
    
    req = prepare_osd_req(dev,
                          0,
                          NULL,
                          0);
    if(req != NULL) {
        req->service_action = OSD_CMD_SET_KEY;
        req->cmd_params.set_key.part_id = part_id;
        req->cmd_params.set_key.key_to_set = key_to_set;
	req->cmd_params.set_key.key_ver = key_ver;
	memcpy(req->cmd_params.set_key.key_id_a,key_id_a,7);
	memcpy(req->cmd_params.set_key.seed_a,seed_a,20);

/* 	if (1==key_to_set) { */
/* 	    req->sec.obj_type=OSD_OBJ_TYPE_ROOT; */
/* 	} else { */
/* 	    req->sec.obj_type=OSD_OBJ_TYPE_PART; */
/* 	} */
/* 	req->sec.part_id=part_id; */
    }

    return req;
}
EXPORT_SYMBOL(prepare_osd_set_key);

struct osd_req_t * prepare_osd_debug(osd_dev_handle_t dev, 
				     uint32_t data_out_len,
				     uint32_t data_in_len,
				     char * data_out,
				     char * data_in)
{
    osd_req_t * req;

    osd_assert(0==data_in_len);

    //fix!!! currently supports only sending debug data to target
    //we need to improve osd_req to support bidi commands except attributes
    
    req = prepare_osd_req(dev,
                          0,
                          data_out,
                          data_out_len);
    
    if(req != NULL) {
        req->service_action = OSD_CMD_DEBUG;
        req->cmd_params.debug.data_out_len=data_out_len;
        req->cmd_params.debug.data_in_len=data_in_len;

/* 	req->sec.obj_type=OSD_OBJ_TYPE_ROOT; */
/* 	req->sec.part_id=0; */
/* 	req->sec.obj_id=0; */
    }

    return req;
}
EXPORT_SYMBOL(prepare_osd_debug);

int prepare_get_attr_list_add_entry( osd_req_t * req,
                                     uint32_t page_num,
                                     uint32_t attr_num,
                                     uint32_t retrieved_attr_len)
{
    RC_t rc;

    if (NULL == req) { 
        PRINT("%s failed: illegal parameters\n", __FUNCTION__);
        return -EINVAL;
    }
    
    osd_assert(req != NULL);
    
    if(req->get_set_attr_fmt != OSD_GET_SET_INVALID_FMT &&
       req->get_set_attr_fmt != OSD_GET_SET_LIST_FMT) {
        OSD_ERROR("OSD requeset has non list attributes format\n");
        return -EINVAL;
    }
    
    req->get_set_attr_fmt = OSD_GET_SET_LIST_FMT;
    
    if(req->new_get_list_len == 0) {
        /* first time we add an entry */

/* 	req->sec.get_mask=0; //init sec mask */

        req->new_get_list_buf = (uint8_t*)os_malloc(OSD_MAX_GET_LIST_LEN);
        if(req->new_get_list_buf == NULL) {
            OSD_ERROR("Can't allocate buffer for attribute list\n");
            return -ENOMEM;
        }
        req->new_get_list_len = OSD_MAX_GET_LIST_LEN;
        
        /* init get attribute */
        rc = osd_attr_list_init_encap( req->new_get_list_buf, 
                                       OSD_MAX_GET_LIST_LEN,
                                       OSD_ATTR_GET_LIST,
                                       &req->get_list_desc);
        
        if(rc != OSD_RC_GOOD) {
            OSD_ERROR("osd_attr_list_init_encap returned an error\n");
            return -1;
        }
    }
    
    rc = osd_attr_num_list_iterate_encap( &req->get_list_desc, 
                                          page_num,attr_num);
    if( rc != OSD_RC_GOOD) {
        OSD_ERROR("osd_attr_num_list_iterate_encap failed\n");
        return -1;
    }

/*     sec_attr_pages_mask_add(&req->sec.get_mask,page_num); */
    
    /* accumulate retrieved get attribute buffer length */
    if(req->retrieved_attr_len == 0) {
        req->retrieved_attr_len += std_sizeof(OSD_ATTR_LIST_HDR_t);
    }
    
    req->retrieved_attr_len += 
	retrieved_attr_len + osd_attr_list_entry_hdr_size(OSD_ATTR_RET_LIST);
        
    return 0;
}
EXPORT_SYMBOL(prepare_get_attr_list_add_entry);


int prepare_set_attr_list_add_entry( osd_req_t * req,
                                     uint32_t page_num,
                                     uint32_t attr_num,
                                     uint16_t attr_len,
                                     const unsigned char *attr_val)
{
    uint8_t *attr_val_buf;
    uint16_t len16;
    int max_len;
    RC_t rc;

    if ((NULL == req) ||
        (attr_len >= OSD_MAX_SET_LIST_LEN-4) ||
        ((attr_len > 0) && (attr_val == NULL))) { 
        PRINT("%s failed: illegal parameters\n", __FUNCTION__);
        return -EINVAL;
    }
    
    osd_assert(req != NULL);
    
    if(req->get_set_attr_fmt != OSD_GET_SET_INVALID_FMT &&
       req->get_set_attr_fmt != OSD_GET_SET_LIST_FMT) {
        OSD_ERROR("OSD requeset has non list attributes format\n");
        return -EINVAL;
    }
    
    req->get_set_attr_fmt = OSD_GET_SET_LIST_FMT;
    
    if(req->new_set_list_len == 0) {
        /* first time we add an entry */

/* 	req->sec.set_mask=0; //init sec mask */

        req->new_set_list_buf = (uint8_t*)os_malloc(OSD_MAX_SET_LIST_LEN);
        if(req->new_set_list_buf == NULL) {
            OSD_ERROR("Can't allocate buffer for attribute list\n");
            return -ENOMEM;
        }
        
        memset(req->new_set_list_buf, 0 ,OSD_MAX_SET_LIST_LEN);
        
        req->new_set_list_len = OSD_MAX_SET_LIST_LEN;
        
        /* init set attribute */
        rc = osd_attr_list_init_encap( req->new_set_list_buf, 
                                       OSD_MAX_SET_LIST_LEN,
                                       OSD_ATTR_SET_LIST,
                                       &req->set_list_desc);
        
        if(rc != OSD_RC_GOOD) {
            OSD_ERROR("osd_attr_list_init_encap returned an error\n");
            return -1;
        }
    }
    
    rc = osd_attr_list_prefetch_val_ptr(&req->set_list_desc, 
					&len16, 
					&attr_val_buf);
    max_len=len16;
    osd_assert(rc == OSD_RC_GOOD);
    
    /* don't allow to overflow original buffer length */
    osd_assert(max_len >= attr_len); 
    
    if (attr_len) {
	osd_assert(attr_val);
	memcpy(attr_val_buf, attr_val, attr_len);
    }
    
    rc = osd_attr_val_list_iterate_encap(&req->set_list_desc, 
					 page_num, attr_num,
					 attr_len,
					 attr_len, attr_val_buf);
    
    osd_assert(rc == OSD_RC_GOOD);

/*     sec_attr_pages_mask_add(&req->sec.set_mask,page_num); */
    
    return 0;
}
EXPORT_SYMBOL(prepare_set_attr_list_add_entry);

int osd_get_result(osd_req_t * req, osd_result_t * res)
{
    osd_sense_t sense;

#ifdef __KERNEL__
    printk("%s: req %p res %p, status %d\n", __func__, req, res, req->status);
#endif
    if ((NULL == req) || (NULL == res)) {
        PRINT("%s failed: illegal parameters\n", __FUNCTION__);
        return -EINVAL;
    }
    
    sense.status = req->status;
    sense.sense_len = OSD_SENSE_MAX_LEN;
    memcpy(sense.sense_buf,req->scsi_desc.sense_buf,sense.sense_len);

    if (osd_sense_decap(&sense, req->service_action, res)) {
	return 0;
    } else {
	return -1;
    }
}
EXPORT_SYMBOL(osd_get_result);

void free_osd_req(osd_req_t * req)
{
    OSD_TRACE(OSD_TRACE_REQ,3, "ENTER\n");
    
    if(req->new_get_list_len > 0 && req->new_get_list_buf != NULL) {
        os_free(req->new_get_list_buf);
    }
    if(req->new_set_list_len > 0 && req->new_set_list_buf != NULL) {
        os_free(req->new_set_list_buf);
    }
    if(req->allocated_retrieved_attr_buf && req->retrieved_attr_buf != NULL) {
        os_free(req->retrieved_attr_buf);
    }
    if(req->allocated_data_out && req->scsi_desc.data_out != NULL) {
        os_free(req->scsi_desc.data_out);
    }
    if(req->allocated_data_in && req->scsi_desc.data_in != NULL) {
        os_free(req->scsi_desc.data_in);
    }
        
    os_free_osd_req(req);
    
    OSD_TRACE(OSD_TRACE_REQ,3, "EXIT\n");
    
    return;
}
EXPORT_SYMBOL(free_osd_req);

osd_ctx_t * create_osd_ctx(uint32_t max_nr_req)
{
    return create_osd_ctx_via_scsi(max_nr_req);
}
EXPORT_SYMBOL(create_osd_ctx);

void destroy_osd_ctx(osd_ctx_t * ctx)
{
    if (ctx != NULL) {
        destroy_osd_ctx_via_scsi(ctx);
    }
}
EXPORT_SYMBOL(destroy_osd_ctx);

#ifdef TEST_AIO
int do_osd_req(osd_req_t * req, 
               osd_ctx_t * ctx,
               void (* done) (osd_req_t *, void *), 
               void * caller_context,
	       uint8_t credential[OSD_CREDENTIAL_SIZE],
	       osd_channel_token_t *channel_token)
{
    int rc;
    
    OSD_TRACE(OSD_TRACE_REQ,3, "ENTER\n");

    if ((NULL == req) || (NULL == done)) {
        PRINT("%s failed: illegal parameters\n", __FUNCTION__);
        return -EINVAL;
    }
    
    /* XXX: have not verified that this is correct; changed the
     * original implementation not to build an SG.  --pw */
    rc = prepare_scsi_param( req, 
                             OSD_DEFAULT_TIMEOUT, 
                             credential, 
                             channel_token);
    if(rc != 0) {
        OSD_ERROR("failed to build  scsi params\n");
        req->status = -1;
        return rc;
    }

    req->done = done;
    req->caller_context = caller_context;
    
    return do_osd_via_scsi(req, ctx, done, caller_context);
}
EXPORT_SYMBOL(do_osd_req);
#endif  /* TEST_AIO */

#if 0  /* unused */
int do_osd_req_timeout(osd_req_t * req, 
                       osd_ctx_t * ctx,
                       void (* done) (osd_req_t *, void *), 
                       void * caller_context,
                       unsigned int timeout,
                       uint8_t credential[OSD_CREDENTIAL_SIZE],
		       osd_channel_token_t *channel_token)
{
    int rc;
    
    OSD_TRACE(OSD_TRACE_REQ,3, "ENTER\n");

    if ((NULL == req) || (NULL == done)) {
        PRINT("%s failed: illegal parameters\n", __FUNCTION__);
        return -EINVAL;
    }
    
    rc = prepare_scsi_param( req, 
                             timeout, 
                             credential, 
                             channel_token);
    if(rc != 0) {
        OSD_ERROR("failed to build  scsi params\n");
        req->status = -1;
        return rc;
    }

    req->done = done;
    req->caller_context = caller_context;
    
    return do_osd_via_scsi(req, ctx, done, caller_context);
}
EXPORT_SYMBOL(do_osd_req_timeout);
#endif

int get_completed_osd_req(osd_ctx_t * ctx, int32_t min_nr, int32_t nr)
{
    return get_completed_osd_req_via_scsi(ctx, min_nr, nr);
}
EXPORT_SYMBOL(get_completed_osd_req);

int wait_osd_req(osd_req_t * req, unsigned int timeout, unsigned int retries,
		 uint8_t credential[OSD_CREDENTIAL_SIZE],
		 osd_channel_token_t *channel_token)
{
    
    int rc;

    if (NULL == req) {
        PRINT("%s failed: illegal parameters\n", __FUNCTION__);
        return -EINVAL;
    }
    
    rc = prepare_scsi_param(req, 
                            timeout,
                            credential, 
                            channel_token);
    if(rc != 0) {
        OSD_ERROR("failed to build  scsi params\n");
        req->status = -1;
        return rc;
    }
    
    /* call to the platform dependent submission call */
    return wait_osd_via_scsi(req, timeout, retries);
}
EXPORT_SYMBOL(wait_osd_req);

int init_extract_attr_from_req(osd_req_t * req)
{
    RC_t rc;
    
    OSD_TRACE(OSD_TRACE_REQ,3,"Enter\n");

    if (NULL == req) {
        PRINT("%s failed: illegal parameters\n", __FUNCTION__);
        return -EINVAL;
    }
    
    /* init get list descriptor to be the descriptor of 
       the retrieved attr list */
    if ( (NULL==req->scsi_desc.data_in) || (0==req->retrieved_attr_len) ) {
	return -EINVAL; //not received attributes
    }
    rc = osd_attr_list_init_decap( req->retrieved_attr_buf, 
				   req->retrieved_attr_len, 
				   OSD_ATTR_RET_LIST,
				   &req->get_list_desc);
    
    if (!OSD_RC_IS_GOOD(rc)) {
	OSD_ERROR("error initializing decap of attr list\n");
	return -1;
    }
    
    OSD_TRACE(OSD_TRACE_REQ,3, " Exit\n");
    return 0;
}
EXPORT_SYMBOL(init_extract_attr_from_req);

int extract_next_attr_from_req(osd_req_t * req, uint32_t * page_num, uint32_t * attr_num, uint16_t * attr_len, uint8_t ** attr_val)
{
    RC_t rc;
    uint16_t full_len;
    
    if (NULL == req) {
        PRINT("%s failed: illegal parameters\n", __FUNCTION__);
        return -EINVAL;
    }

    rc = osd_attr_val_list_iterate_decap(&req->get_list_desc, 
                                         page_num,
                                         attr_num,
					 &full_len,
                                         attr_len,
					 attr_val);
    if(rc != OSD_RC_GOOD) {
	return -1;
    } else if (full_len==OSD_ATTR_UNDEFINED_VAL_LENGTH) {
	*attr_len = full_len;
    } else if (full_len != *attr_len) {
	OSD_ERROR("Error extracting attribute. full size=%d got less (%d).\n",
		  full_len, *attr_len);
        return -1;
    }
    
    return 0;
}
EXPORT_SYMBOL(extract_next_attr_from_req);

int extract_list_from_req(struct osd_req_t * req,
			  uint64_t *total_matches_p,
			  uint64_t *num_ids_retrieved_p,
			  uint64_t *list_of_ids_p[],
			  int      *is_list_of_partitions_p,
			  int      *list_isnt_up_to_date_p,
			  uint64_t *continuation_tag_p,
			  uint32_t *list_id_for_more_p)
{
    uint64_t cont_tag,max_entries;
    uint32_t list_id;
    uint8_t flags;
    uint8_t *data;
    int i;

    if ((NULL == req)                     ||
        (NULL == total_matches_p)         ||
        (NULL == num_ids_retrieved_p)     ||
        (NULL == list_of_ids_p)           ||
        (NULL == is_list_of_partitions_p) ||
        (NULL == list_isnt_up_to_date_p)  ||
        (NULL == continuation_tag_p)      ||
        (NULL == list_id_for_more_p)) {
        PRINT("%s failed: illegal parameters\n", __FUNCTION__);
        return -EINVAL;
    }

    
    /* init get list descriptor to be the descriptor of 
       the retrieved attr list */
    if ( (NULL==req->cmd_data) || 
	 (req->cmd_data_direction != DMA_FROM_DEVICE) ||
	 (OSD_CMD_LIST!=req->service_action) ) {
	OSD_ERROR("invalid req to extract list from.\n");
	return -EINVAL; //not received list
    }

    if (req->status) {
	OSD_ERROR("cannot extract list from unsuccesfull command.\n");
	return -EINVAL;
    }

    if (!osd_decap_cmd_list_data_hdr(req->cmd_data,
				     total_matches_p,
				     &cont_tag,&list_id,&flags) ) {
	OSD_ERROR("error decaping list results.\n");
	return -1;
    }
   
    max_entries=osd_calc_max_list_entries_in_len(req->cmd_params.list.alloc_len);
    if (*total_matches_p>max_entries) {
	*num_ids_retrieved_p=max_entries;
    } else {
	*num_ids_retrieved_p=*total_matches_p;
    }

    *is_list_of_partitions_p= (flags&LIST_ROOT_BIT_VAL) ? 1 : 0;
    *list_isnt_up_to_date_p= (flags&LIST_LSTCHG_BIT_VAL) ? 1 : 0;
    if (cont_tag) {
	*continuation_tag_p = cont_tag;
	*list_id_for_more_p = list_id;
    } else {
	*continuation_tag_p =0;
	*list_id_for_more_p = 0;
    }
    
    if (0==*num_ids_retrieved_p) {
	*list_of_ids_p = NULL;
    } else {
	data = ( ((uint8_t*)req->cmd_data)+sizeof(OSD_CMD_LIST_DATA_HDR_t));
	*list_of_ids_p = (uint64_t*)data;
	for (i = 0 ; i < *num_ids_retrieved_p ; i++) {
	  (*list_of_ids_p)[i] = NTOHLL((*list_of_ids_p)[i]);
	}
    }

    return 0;
}
EXPORT_SYMBOL(extract_list_from_req);

int prepare_set_timestamp_bypass_mode(osd_req_t *req,uint8_t bypass_mode)
{

    if (NULL == req) {
        PRINT("%s failed: illegal parameters\n", __FUNCTION__);
        return -EINVAL;
    }
    
    osd_assert(req);
    if (bypass_mode&OSD_CDB_USE_TIMESTAMP_CTL) {
	OSD_ERROR("illegal timstamp bypass mode=%x. should not use %x bits.\n",
		  (int)bypass_mode,(int)OSD_CDB_USE_TIMESTAMP_CTL);
	return -EINVAL;
    }
    req->timestamp_ctl=OSD_CDB_USE_TIMESTAMP_CTL|(bypass_mode);
    return 0;
}
EXPORT_SYMBOL(prepare_set_timestamp_bypass_mode);

void osd_req_module_init(void) {
}
EXPORT_SYMBOL(osd_req_module_init);

uint64_t extract_id_from_inquiry_data(char* data)
{
    uint64_t id          = 0;
    int      page_len    = 0;
    uint8_t* id_list_p   = NULL;
    int      i           = 0;
    uint8_t* id_desc_p   = NULL;
    int      id_len      = 0;
    int      code_set    = 0;
    int      id_type     = 0;
    int      association = 0;
    int      naa         = 0;
    
    if (NULL == data) {
        PRINT("%s failed: illegal parameters\n", __FUNCTION__);
        return 0;
    }
    
    if (((uint8_t *) data)[1] != 0x83) {
        PRINT("Wrong VPD page\n");
        return 0;
    }
    
    id_list_p = (uint8_t *) &(data[4]);
    page_len = NTOHS(*(uint16_t*)&data[2]);

    i = 0;
    while(i < page_len) {
        id_desc_p = &(id_list_p[i]);
        id_len = id_desc_p[3];
        code_set = id_desc_p[0] & 0x0f;
        id_type = id_desc_p[1] & 0x0f;
        
        /* check if id associated with the LUN */
        association = id_desc_p[1] & 0x30;
        if (association != 0) {
            goto next;
        }

        switch (code_set) {
        case 1:  /* binary value */
            switch (id_type) {
            case 0: /* vendor specific */
                memcpy(&id, &id_desc_p[4], (id_len < 8 ? id_len : 8));
                return NTOHLL(id);

            case 2: /* EUI-64 */
                return NTOHLL(*(uint64_t*)&id_desc_p[4]);
                
            case 3: /* NAA */
                naa = id_desc_p[4] & 0xf0;
                if (0x20 == naa) {
                    id_desc_p[4] &= 0x0f;
                    return NTOHLL(*(uint64_t*)&id_desc_p[4]);
                }
                goto next;

            default:
                goto next;
            }

        case 2:  /* ascii value */
            goto next;
            
        default:
            PRINT("Illegal code set in inquiry 0x83 page\n");
            return 0;
        }

    next:
        i += (id_len+4);
    } /* end of while */

    return 0;
}
EXPORT_SYMBOL(extract_id_from_inquiry_data);

/* return pointer to the begining of system id */
uint8_t* extract_system_id_from_inquiry_data(uint8_t* data)
{
    int      page_len  = 0;
    uint8_t* id_list_p = NULL;
    int      i         = 0;
    uint8_t* id_desc_p = NULL;
    int      id_len    = 0;
    int      code_set  = 0;
    int      id_type   = 0;
    int      association = 0;
    int      naa         = 0;
    
    if (NULL == data) {
        PRINT("%s failed: illegal parameters\n", __FUNCTION__);
        return NULL;
    }
    
    if (data[1] != 0x83) {
        PRINT("Wrong VPD page\n");
        return NULL;
    }
    
    id_list_p = &(data[4]);
    page_len = NTOHS(*(uint16_t*)&data[2]);

    i = 0;
    while(i < page_len) {
        id_desc_p = &(id_list_p[i]);
        id_len = id_desc_p[3];
        code_set = id_desc_p[0] & 0x0f;
        id_type = id_desc_p[1] & 0x0f;
        
        /* check if id associated with the LUN */
        association = id_desc_p[1] & 0x30;
        if (association != 0) {
            goto next;
        }

        switch (code_set) {
        case 1:  /* binary value */
            switch (id_type) {
            case 2: /* EUI-64 */
                return id_desc_p;
            case 3: /* NAA */
                naa = id_desc_p[4] & 0xf0;
                if (0x20 == naa) {
                    id_desc_p[4] &= 0x0f;
                    return id_desc_p;
                }
                goto next;
                
            default:
                goto next;
            }

        case 2:  /* ascii value */
            goto next;
            
        default:
            PRINT("Illegal code set in inquiry 0x83 page\n");
            return NULL;
        }

    next:
        i += (id_len+4);
    } /* end of while */

    return NULL;
}

int get_osd_channel_token(osd_dev_handle_t dev,
			  osd_channel_token_t * channel_token)
{
    char data[VPD_PAGE_MAX_LEN] = {0};
    struct osd_req_t * req;
    osd_result_t res;
    int channel_token_len = 0;
    int rc=0;

    req = (struct osd_req_t*) prepare_osd_inquiry(dev, VPD_PAGE_MAX_LEN,
                                                  data,
                                                  0xB1);
    if (req == NULL) {
        return -1;
    }

    wait_osd_req(req, 5, 1, NULL, NULL);

    osd_get_result(req, &res);

    if (! OSD_RC_IS_GOOD(res.rc)) {
	OSD_ERROR("failed getting channel token\n");
        return -1;
    }

    /* extrace channel token */
    channel_token_len = NTOHS(*(uint16_t *)&(data[2]));

    if (channel_token_len > (OSD_CHANNEL_TOKEN_MAX_LEN)) {
	OSD_ERROR("got channel security token len=%d > max supported=%d.",
		  channel_token_len, OSD_CHANNEL_TOKEN_MAX_LEN );
        channel_token_len = OSD_CHANNEL_TOKEN_MAX_LEN;
	rc=-1;
    }

    osd_assert(channel_token_len+4 <= VPD_PAGE_MAX_LEN);
    memcpy(channel_token->channel_token_buf, &(data[4]), channel_token_len);
    channel_token->channel_token_length = channel_token_len;

    if (channel_token_len<16) {
	OSD_ERROR("received chanel token has illegal length: %d. should be >=16\n",
		  channel_token_len);
	rc=-1;
    }

    free_osd_req(req);

    return rc;
}
EXPORT_SYMBOL(get_osd_channel_token);


int get_osd_system_id(osd_dev_handle_t dev, uint8_t system_id[20])
{
    char data[VPD_PAGE_MAX_LEN] = {0};
    struct osd_req_t * req;
    osd_result_t res;
    char* system_id_start = NULL;
    int id_len;
    
    req = prepare_osd_inquiry(dev, VPD_PAGE_MAX_LEN, data, 0x83);
    if (req == NULL) {
        return -1;
    }
    
    wait_osd_req(req, 5, 1, NULL, NULL);

    osd_get_result(req, &res);
    if (! OSD_RC_IS_GOOD(res.rc)) {
        return -1;
    }

    /* extract system id */
    system_id_start = (char*)extract_system_id_from_inquiry_data((uint8_t*)data);
    if (system_id_start == NULL) {
        return -1;
    }

    id_len = 4 + system_id_start[3];
    if (id_len>20) 
	id_len=20;
    memset(system_id, 0, 20);
    memcpy(system_id, system_id_start, id_len);

    free_osd_req(req);

    return 0;
}
EXPORT_SYMBOL(get_osd_system_id);

