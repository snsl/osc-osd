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

#ifndef OSD_SENSE_H
#define OSD_SENSE_H

#include "osd.h"

//tbd!!! these values should come from some scsi h file
#define OSD_SENSE_NO_INFO 0x0000
#define OSD_SENSE_INVALID_FIELD_IN_CDB 0x2400
#define OSD_SENSE_INVALID_FIELD_IN_PARAMETER_LIST 0x2600
#define OSD_SENSE_QUOTA_ERROR 0x5507
#define OSD_SENSE_READ_PAST_END_OF_USER_OBJECT 0x3B17
#define OSD_SENSE_PARTITION_NOT_EMPTY 0x2C0A
#define OSD_SENSE_INVALID_OPCODE 0x2000
//sense for future usage:
#define OSD_SENSE_SEC_AUDIT_FROZEN 0x2404
#define OSD_SENSE_SEC_KEY_FROZEN 0x2405
#define OSD_SENSE_UNSUPPORTED_LUN 0x2500

#define OSD_SENSE_INTERNAL_ERROR 0x4400
#define OSD_SENSE_OP_IN_PROGRESS 0x0016

#define OSD_SCSI_NO_SENSE            0x00
#define OSD_SCSI_RECOVERED_ERROR     0x01
#define OSD_SCSI_ILLEGAL_REQUEST     0x05
#define OSD_SCSI_DATA_PROTECT        0x07

#define OSD_SENSE_RESPONSE_CODE 0x72

typedef enum {
    OSD_SENSE_DESC_CMD_INFO=1,    //command specific information descriptor
    OSD_SENSE_DESC_KEY_SPF=2, //sense key specific sense data
    OSD_SENSE_DESC_OSD_ERROR=6, //osd error identification descriptor
    OSD_SENSE_DESC_OSD_ATTR=8, //osd attribute identification descriptor
    OSD_SENSE_DESC_NONE=0xffff,
} osd_sense_desc_key_t;

typedef struct {
    uint8_t status;
    int sense_len;
    uint8_t sense_buf[OSD_SENSE_MAX_LEN];
} osd_sense_t, Osd_sense;



static inline void osd_sense_encap( osd_sense_t *sense_p, //output
				    const osd_result_t *result_p);

static inline osd_bool_t osd_sense_decap( const osd_sense_t *sense_p,
					  uint16_t service_action,
					  osd_result_t *result_p);

//variable length sense data format. see SPC3 r16 section 4.5.2.1
typedef struct {
    uint8_t resp_code; //0x72
    uint8_t sense_key; //4 lower bits
    uint8_t asc;
    uint8_t ascq;
    uint8_t reserved[3];
    uint8_t add_len; //n-7 = 0 if no additional descriptors
} OSD_PACKED osd_sense_hdr_t;

//general descriptor header. see SPC3 4.5.2.1
typedef struct {
    uint8_t key; 
    uint8_t add_len; //descriptor length - 2
} OSD_PACKED osd_sense_desc_hdr_t;


//OSD error identification descriptor. see T10 OSD starndard rev 10 section 4.14.2.1
typedef struct {
    uint8_t key; //6=OSD error id
    uint8_t add_len; //0x1E
    uint8_t reserved[6];

    //command functions state masks - use OSD_PHASE_... constants from osd.h
    uint32_t not_initiated_cmd_funcs;
    uint32_t completed_cmd_funcs;

    //command object identification
    uint64_t partition_id;
    uint64_t object_id;
} OSD_PACKED osd_sense_osd_error_desc_t;

//values for flags byte of sense key specific field pointer. see SPC3 4.5.2.4.2
#define OSD_SENSE_DESC_KEY_SPF_FLAGS_CDB_FIELD 0xC0 //invalid cdb field
#define OSD_SENSE_DESC_KEY_SPF_FLAGS_DATA_FIELD 0x80 //invalid data-out field

//SENSE KEY SPECIFIC descriptor. see spc3r16 4.5.2.4
typedef struct {
    uint8_t key;     //=2
    uint8_t add_len; //=6
    uint8_t reserved[2]; 
    uint8_t flags; //use OSD_SENSE_DESC_KEY_SPF_FLAGS_.. defines
    uint16_t field_offset;
    uint8_t reserved1;
} OSD_PACKED osd_sense_key_spf_desc_t;


//command specific information sense data descriptor. see spc3r16 4.5.2.2
//used to tell number of bytes actually read in READ PAST END OF OBJECT
typedef struct {
    uint8_t key;     //=1
    uint8_t add_len; //=0x0A
    uint8_t reserved[2];
    uint64_t value;
} OSD_PACKED osd_sense_cmd_info_desc_t;

//Attribute id sense data descriptor. see T10 OSD standard r10 4.14.2.3
typedef struct {
    uint8_t key;     //=8
    uint8_t add_len; //=num attributes*8 - 2
    uint8_t reserved[2];
    uint32_t page_num;
    uint32_t attr_num;
} OSD_PACKED osd_sense_osd_attr_desc_t;

static inline void osd_sense_encap_hdr(osd_sense_t *sense_p,
				       osd_rc_t rc);
					       
static inline osd_bool_t osd_sense_decap_hdr(const osd_sense_t *sense_p,
				       osd_rc_t *rc_p);

static inline void osd_sense_add_num_bytes_read(
    osd_sense_t *sense_p,
    uint64_t num_bytes_actually_read);
					       
static inline void osd_sense_desc_get_num_bytes_read(
    const void *descriptor_p,
    uint64_t *num_bytes_actually_read_p);

static inline void osd_sense_add_osd_error_id(
    osd_sense_t *sense_p,
    uint32_t not_initiated_cmd_funcs,
    uint32_t completed_cmd_funcs,
    uint64_t partition_id,
    uint64_t object_id);
					       
static inline void osd_sense_desc_extract_osd_error_id(
    const void *descriptor_p,
    uint32_t *not_initiated_cmd_funcs_p,
    uint32_t *completed_cmd_funcs_p,
    uint64_t *partition_id_p,
    uint64_t *object_id_p);

static inline void osd_sense_add_cdb_field_offset(
    osd_sense_t *sense_p,
    uint16_t invalid_cdb_field_offset);

static inline void osd_sense_desc_extract_cdb_field_offset(
    const void *descriptor_p,
    uint16_t *invalid_cdb_field_offset_p);

static inline void osd_sense_add_data_field_offset(
    osd_sense_t *sense_p,
    uint16_t invalid_data_field_offset);

static inline void osd_sense_desc_extract_data_field_offset(
    const void *descriptor_p,
    uint16_t *invalid_data_field_offset_p);

static inline void osd_sense_add_attr_id(
    osd_sense_t *sense_p,
    uint32_t page_num, uint32_t attr_num);

static inline void osd_sense_desc_extract_attr_id(
    const void *descriptor_p,
    uint32_t *page_num_p, uint32_t *attr_num_p);

static inline uint16_t osd_cdb_field_to_offset(osd_bad_cdb_field_t bad_field );


static inline osd_bad_cdb_field_t osd_cdb_offset_to_field(uint16_t service_action,
							  uint16_t ofs);

//set up header of sense_buffer according to osd_rc_t value
//if additional sense data is needed, it should be added using the osd_sense_add_...
//api's
static inline void osd_sense_encap_hdr(osd_sense_t *sense_p,
				       osd_rc_t rc)
{
    osd_sense_hdr_t *sense_hdr_p;
    uint16_t add_sense_code;
    int is_non_recoverable;

    osd_assert(sense_p);
    sense_hdr_p = (osd_sense_hdr_t*)sense_p->sense_buf;
    osd_assert(sense_hdr_p);
    memset(sense_hdr_p, 0, sizeof(osd_sense_hdr_t));

    if (OSD_RC_GOOD==rc) {
	sense_p->status=SCSI_GOOD;
	OSD_TRACE(OSD_TRACE_T10,2, 
		  "sense enc: status=0x%x rc=%s\n",
		  sense_p->status,osd_rc_name(rc));
	sense_p->sense_len=0;
	is_non_recoverable=0;
	return;
    }
    //else...
    is_non_recoverable=1;
    sense_p->status = SCSI_CHECK_CONDITION;
    sense_hdr_p->sense_key = OSD_SCSI_ILLEGAL_REQUEST;
    add_sense_code=OSD_SENSE_NO_INFO; //default to be overwritten
    sense_hdr_p->resp_code=OSD_SENSE_RESPONSE_CODE;
    sense_hdr_p->add_len=0;
    switch (rc) {
    case OSD_RC_INVALID_CDB:
     	add_sense_code = OSD_SENSE_INVALID_FIELD_IN_CDB;
	break;
    case OSD_RC_INVALID_DATA_OUT:
	add_sense_code = OSD_SENSE_INVALID_FIELD_IN_PARAMETER_LIST;
	break;
    case OSD_RC_QUOTA_ERROR:
	sense_hdr_p->sense_key= OSD_SCSI_DATA_PROTECT;
	add_sense_code = OSD_SENSE_QUOTA_ERROR;
	break;
    case OSD_RC_INVALID_OPCODE:
	add_sense_code = OSD_SENSE_INVALID_OPCODE;
	break;
    case OSD_RC_READ_PAST_END_OF_OBJECT :
	sense_hdr_p->sense_key=OSD_SCSI_RECOVERED_ERROR;
	add_sense_code = OSD_SENSE_READ_PAST_END_OF_USER_OBJECT;
	is_non_recoverable=0;
	break;
    case OSD_RC_NONEMPTY_PARTITION:
	add_sense_code = OSD_SENSE_PARTITION_NOT_EMPTY;
	break;
    case OSD_RC_MEDIA_ERROR:
	add_sense_code = 0;//tbd!!!
	break;
    case OSD_RC_FROZEN_AUDIT:
	add_sense_code = OSD_SENSE_SEC_AUDIT_FROZEN;
	break;
    case OSD_RC_FROZEN_KEY:
	add_sense_code = OSD_SENSE_SEC_KEY_FROZEN;
	break;

    default:
	osd_assert(0);
	break;
    }
    sense_hdr_p->asc=add_sense_code>>8;
    sense_hdr_p->ascq=add_sense_code&0xff;

    sense_p->sense_len=sense_hdr_p->add_len+8;
    osd_assert(sense_p->sense_len<=sizeof(osd_sense_hdr_t));
    osd_assert(sense_p->sense_len<=OSD_SENSE_MAX_LEN);

    OSD_TRACE(OSD_TRACE_T10,1, 
	      "sense enc: status=0x%x rc=%s\n",
	      sense_p->status,osd_rc_name(rc));
    OSD_TRACE(OSD_TRACE_T10,2, 
	      "sense enc: key=0x%x add_code=0x%x\n",
	      sense_hdr_p->sense_key ,add_sense_code); 
}
					       
static inline osd_bool_t osd_sense_decap_hdr(const osd_sense_t *sense_p,
				       osd_rc_t *rc_p)
{
    const osd_sense_hdr_t *sense_hdr_p;
    uint16_t add_sense_code;

    osd_assert(rc_p);
    osd_assert(sense_p);
    sense_hdr_p = (osd_sense_hdr_t*)sense_p->sense_buf;
    osd_assert(sense_hdr_p);

    *rc_p = OSD_RC_UNKNOWN_ERROR;

    OSD_TRACE(OSD_TRACE_T10,2,"sense dec: status=0x%x \n",sense_p->status);
    if (SCSI_GOOD==sense_p->status) {
	*rc_p=OSD_RC_GOOD;
	return OSD_TRUE;
    }
    //else...


    if (SCSI_CHECK_CONDITION != sense_p->status) {
	//don't know to handle it . . .
	//let rc remain unknown error and return FALSE
	return OSD_FALSE;
    }
    
    add_sense_code=((uint16_t)sense_hdr_p->asc<<8)+(sense_hdr_p->ascq);

    OSD_TRACE(OSD_TRACE_T10,2, 
	      "sense dec: key=0x%x add_code=0x%x \n",
	      sense_hdr_p->sense_key ,add_sense_code);
    
    if (sense_p->sense_len<sizeof(osd_sense_hdr_t)) {
	return OSD_FALSE;
    }

    if ( (sense_p->sense_len<8) || //minimal sense size we can parse
	 ((sense_hdr_p->add_len+8)>sense_p->sense_len) || //not all sense bytes delivered
	 (sense_hdr_p->resp_code!=OSD_SENSE_RESPONSE_CODE) )
    {
	OSD_TRACE(OSD_TRACE_T10,1,
		  "sense format error. got len %d add_len+8=%d resp_code=0x%x\n",
		  sense_p->sense_len,(int)sense_hdr_p->add_len+8,(int)sense_hdr_p->resp_code);
	return OSD_FALSE;
    }

    switch (sense_hdr_p->sense_key) {
    case OSD_SCSI_ILLEGAL_REQUEST: {
	switch (add_sense_code) {
	case OSD_SENSE_INVALID_FIELD_IN_CDB:
	    *rc_p=OSD_RC_INVALID_CDB;
	    break;
	case OSD_SENSE_INVALID_FIELD_IN_PARAMETER_LIST:
	    *rc_p=OSD_RC_INVALID_DATA_OUT;
	    break;
	case OSD_SENSE_INVALID_OPCODE:
	    *rc_p=OSD_RC_INVALID_OPCODE;
	    break;
	case OSD_SENSE_PARTITION_NOT_EMPTY:
	    *rc_p=OSD_RC_NONEMPTY_PARTITION;
	    break;
	default:
	    OSD_TRACE(OSD_TRACE_T10,1,"invalid sense code 0x%x.\n",add_sense_code);
	    *rc_p=OSD_RC_UNKNOWN_ERROR; //default error code
	    return OSD_FALSE;
	}
	break;
    }
    case OSD_SCSI_DATA_PROTECT: {
	switch (add_sense_code) {
	case OSD_SENSE_QUOTA_ERROR:
	    *rc_p=OSD_RC_QUOTA_ERROR;
	    break;
	default:
	    OSD_TRACE(OSD_TRACE_T10,1,"invalid sense code 0x%x.\n",add_sense_code);
	    *rc_p=OSD_RC_UNKNOWN_ERROR; //default error code
	    return OSD_FALSE;
	}
	break;
    }
    case OSD_SCSI_RECOVERED_ERROR:
	switch (add_sense_code) {
	case OSD_SENSE_READ_PAST_END_OF_USER_OBJECT:
	    *rc_p=OSD_RC_READ_PAST_END_OF_OBJECT;
	    break;
	default:
	    OSD_TRACE(OSD_TRACE_T10,1,"invalid sense code 0x%x.\n",add_sense_code);
	    *rc_p=OSD_RC_UNKNOWN_ERROR; //default error code
	    return OSD_FALSE;
	}
	break;
    default:
	OSD_TRACE(OSD_TRACE_T10,1,"invalid sense key 0x%x.\n",(int)sense_hdr_p->sense_key);
	*rc_p=OSD_RC_UNKNOWN_ERROR; //default error code
	return OSD_FALSE;
    }

    OSD_TRACE(OSD_TRACE_T10,1,"error found. status=0x%x rc=%s\n",
	      sense_p->status, osd_rc_name(*rc_p));
    return OSD_TRUE;
}

#define namecase(symbol) case symbol: return #symbol

static inline const char * osd_sense_desc_name(osd_rc_t rc) {
    switch (rc) {
    namecase(OSD_SENSE_DESC_CMD_INFO);
    namecase(OSD_SENSE_DESC_KEY_SPF);
    namecase(OSD_SENSE_DESC_OSD_ERROR);
    namecase(OSD_SENSE_DESC_OSD_ATTR);
    namecase(OSD_SENSE_DESC_NONE);
    default: osd_assert(0); return NULL;
    }
}

#undef namecase
static inline void osd_sense_desc_print(const char * prefix, const void *desc_p)
{
    const osd_sense_desc_hdr_t *hdr_p = (osd_sense_desc_hdr_t *)desc_p;
    osd_trace(OSD_TRACE_T10,2,
	      "%s SENSE DESCRIPTOR, KEY=0x%x=%s, LEN=%d\n",
	      prefix,
	      hdr_p->key,
              osd_sense_desc_name((osd_rc_t)hdr_p->key),
	      hdr_p->add_len+2);
}

//append sense descriptor in desc_p to sense_data
//length of descriptor is extracted from the descriptor
static inline void osd_sense_add_desc(osd_sense_t *sense_p,
				      const void * desc_p)
{
    char * dest_buf_p;
    const osd_sense_desc_hdr_t *hdr_p = (osd_sense_desc_hdr_t *)desc_p;
    int desc_len;
    osd_sense_hdr_t *sense_hdr_p;

    osd_assert(sense_p);
    osd_assert(desc_p);
    osd_assert(sense_p->status!=0);
    osd_assert(sense_p->sense_buf);
    osd_assert(sense_p->sense_len>=sizeof(osd_sense_hdr_t));

    osd_sense_desc_print("adding",desc_p);
    
    desc_len = 2 + hdr_p->add_len;
    dest_buf_p = (char*)&(sense_p->sense_buf[sense_p->sense_len]);

    sense_p->sense_len += desc_len;
    osd_assert(sense_p->sense_len<=OSD_SENSE_MAX_LEN);

    sense_hdr_p = (osd_sense_hdr_t*)sense_p->sense_buf;
    osd_assert(sense_hdr_p->add_len + desc_len < 255);
    sense_hdr_p->add_len += desc_len;

    memcpy(dest_buf_p, desc_p, desc_len);
}

//copy to desc_p the next sense descriptor after *position_p.
//desc_len_p is set to the length of the copied descriptor or zero if no desc found
//*position_p is updated to the position after the copied descriptor
static inline osd_bool_t osd_sense_extract_next_desc(const osd_sense_t *sense_p,
					       int * position_p,
					       char desc_p[OSD_SENSE_MAX_LEN],
					       osd_sense_desc_key_t *desc_key_p,
					       int * desc_len_p)
{
    osd_sense_desc_hdr_t *hdr_p;
    osd_sense_hdr_t *sense_hdr_p;
    int sense_len;

    osd_assert(sense_p);
    osd_assert(position_p);
    osd_assert(desc_p);
    osd_assert(desc_key_p);
    osd_assert(desc_len_p);
    osd_assert(sense_p->sense_buf);

    if (0==*position_p) *position_p+=sizeof(osd_sense_hdr_t);

    sense_hdr_p = (osd_sense_hdr_t*)sense_p->sense_buf;
    sense_len = 8+sense_hdr_p->add_len;
    if (sense_len > sense_p->sense_len) {
	sense_len=sense_p->sense_len;
    }

    if (*position_p < sense_len) {
	hdr_p = (osd_sense_desc_hdr_t*)&(sense_p->sense_buf[*position_p]);
	*desc_len_p = 2 + hdr_p->add_len;
	*desc_key_p = (osd_sense_desc_key_t)hdr_p->key;
	*position_p += *desc_len_p;
	if (*position_p > sense_len) { //descriptor is truncated


	    //reverse last increment
	    *position_p -= *desc_len_p; 

	    //calc truncated descriptor length
	    *desc_len_p = (sense_len - (*position_p));

	    *position_p = sense_len;
	}
	memcpy(desc_p, hdr_p, *desc_len_p);
	return OSD_TRUE;
    } else {
	*desc_len_p=0;
	*desc_key_p=OSD_SENSE_DESC_NONE;
	return OSD_FALSE;
    }
}

static inline osd_bool_t osd_sense_extract_desc_by_key(
    const osd_sense_t *sense_p,
    osd_sense_desc_key_t required_desc_key,
    char desc_p[OSD_SENSE_MAX_LEN],
    int * desc_len_p)
{
    int position=0;
    osd_sense_desc_key_t key;
    osd_bool_t rc=OSD_TRUE;
    
    while ( rc && (position<sense_p->sense_len)) {
	rc=osd_sense_extract_next_desc(sense_p,
				       &position,
				       desc_p,
				       &key,
				       desc_len_p);
	if (key==required_desc_key) {
	    osd_sense_desc_print("Found",desc_p);
	    return OSD_TRUE;
	}
    }
    //if we got here, we didn't find any match
    return OSD_FALSE;
}

static inline void osd_sense_add_num_bytes_read(
    osd_sense_t *sense_p,
    uint64_t num_bytes_actually_read)
{
    osd_sense_cmd_info_desc_t desc;
    int desc_len=sizeof(osd_sense_cmd_info_desc_t);
    
    memset(&desc, 0, desc_len);

    desc.key=OSD_SENSE_DESC_CMD_INFO;
    desc.add_len=desc_len-2;
    desc.value=HTONLL(num_bytes_actually_read);

    osd_sense_add_desc(sense_p, &desc);
}

static inline void osd_sense_desc_get_num_bytes_read(
    const void *descriptor_p,
    uint64_t *num_bytes_actually_read_p)
{
    const osd_sense_cmd_info_desc_t *desc_p = (osd_sense_cmd_info_desc_t *)descriptor_p;
    int desc_len=sizeof(osd_sense_cmd_info_desc_t);

    osd_assert(desc_p);
    osd_assert(desc_p->key==OSD_SENSE_DESC_CMD_INFO);
    if (desc_p->add_len+2!=desc_len) {
	OSD_TRACE(OSD_TRACE_T10,1,"Invalid length of cmd_info descriptor %d. should be %d\n",
		  (int)desc_p->add_len+2,
		  desc_len);
	return;
    }

    *num_bytes_actually_read_p = NTOHLL(desc_p->value);

}

static inline void osd_sense_add_osd_error_id(
    osd_sense_t *sense_p,
    uint32_t not_initiated_cmd_funcs,
    uint32_t completed_cmd_funcs,
    uint64_t partition_id,
    uint64_t object_id)
{
    osd_sense_osd_error_desc_t desc;
    int desc_len=sizeof(osd_sense_osd_error_desc_t);
    
    memset(&desc, 0, desc_len);

    desc.key=OSD_SENSE_DESC_OSD_ERROR;
    desc.add_len=desc_len-2;
    desc.not_initiated_cmd_funcs = HTONL(not_initiated_cmd_funcs);
    desc.completed_cmd_funcs = HTONL(completed_cmd_funcs);
    desc.partition_id = HTONLL(partition_id);
    desc.object_id = HTONLL(object_id);

    osd_sense_add_desc(sense_p, &desc);

}
					       
static inline void osd_sense_desc_extract_osd_error_id(
    const void *descriptor_p,
    uint32_t *not_initiated_cmd_funcs_p,
    uint32_t *completed_cmd_funcs_p,
    uint64_t *partition_id_p,
    uint64_t *object_id_p)
{
    const osd_sense_osd_error_desc_t *desc_p = (osd_sense_osd_error_desc_t *)descriptor_p;
    int desc_len=sizeof(osd_sense_osd_error_desc_t);

    osd_assert(desc_p);
    osd_assert(desc_p->key==OSD_SENSE_DESC_OSD_ERROR);
    if (desc_p->add_len+2!=desc_len) {
	OSD_TRACE(OSD_TRACE_T10,1,"Invalid length of osd_error_id descriptor %d. should be %d\n",
		  (int)desc_p->add_len+2,
		  desc_len);
	return;
    }

    *not_initiated_cmd_funcs_p = NTOHL(desc_p->not_initiated_cmd_funcs);
    *completed_cmd_funcs_p = NTOHL(desc_p->completed_cmd_funcs);
    *partition_id_p=NTOHLL(desc_p->partition_id);
    *object_id_p=NTOHLL(desc_p->object_id);
}

static inline void osd_sense_add_cdb_field_offset(
    osd_sense_t *sense_p,
    uint16_t invalid_cdb_field_offset)
{
    osd_sense_key_spf_desc_t desc;
    int desc_len=sizeof(osd_sense_key_spf_desc_t);
    
    memset(&desc, 0, desc_len);

    desc.key = OSD_SENSE_DESC_KEY_SPF;
    desc.add_len = desc_len-2;
    desc.flags = OSD_SENSE_DESC_KEY_SPF_FLAGS_CDB_FIELD;
    desc.field_offset = HTONS(invalid_cdb_field_offset);

    osd_sense_add_desc(sense_p, &desc);
}

static inline void osd_sense_desc_extract_cdb_field_offset(
    const void *descriptor_p,
    uint16_t *invalid_cdb_field_offset_p)
{
    const osd_sense_key_spf_desc_t *desc_p = (osd_sense_key_spf_desc_t *)descriptor_p;
    int desc_len=sizeof(osd_sense_key_spf_desc_t);

    osd_assert(desc_p);
    osd_assert(desc_p->key == OSD_SENSE_DESC_KEY_SPF);
    if (desc_p->add_len+2!=desc_len) {
	OSD_TRACE(OSD_TRACE_T10,1,"Invalid length of key_spf descriptor %d. should be %d\n",
		  (int)desc_p->add_len+2,
		  desc_len);
	return;
    }
    if ((desc_p->flags & 0xF0) != OSD_SENSE_DESC_KEY_SPF_FLAGS_CDB_FIELD) {
	OSD_TRACE(OSD_TRACE_T10,1,"Invalid flags of cdb_ofs key_spf descriptor 0x%x.\n",
		  (int)desc_p->flags);
	return;
    }

    *invalid_cdb_field_offset_p = NTOHS(desc_p->field_offset);
}


static inline void osd_sense_add_data_field_offset(
    osd_sense_t *sense_p,
    uint16_t invalid_data_field_offset)
{
    osd_sense_key_spf_desc_t desc;
    int desc_len=sizeof(osd_sense_key_spf_desc_t);
    
    memset(&desc, 0, desc_len);

    desc.key=OSD_SENSE_DESC_KEY_SPF;
    desc.add_len=desc_len-2;
    desc.flags = OSD_SENSE_DESC_KEY_SPF_FLAGS_DATA_FIELD;
    desc.field_offset =HTONS(invalid_data_field_offset);

    osd_sense_add_desc(sense_p, &desc);
}

static inline void osd_sense_desc_extract_data_field_offset(
    const void *descriptor_p,
    uint16_t *invalid_data_field_offset_p)
{
    const osd_sense_key_spf_desc_t *desc_p = (osd_sense_key_spf_desc_t *)descriptor_p;
    int desc_len=sizeof(osd_sense_key_spf_desc_t);

    osd_assert(desc_p);
    osd_assert(desc_p->key == OSD_SENSE_DESC_KEY_SPF);
    if (desc_p->add_len+2!=desc_len) {
	OSD_TRACE(OSD_TRACE_T10,1,"Invalid length of key_spf descriptor %d. should be %d\n",
		  (int)desc_p->add_len+2,
		  desc_len);
	return;
    }
    if ((desc_p->flags & 0xF0) != OSD_SENSE_DESC_KEY_SPF_FLAGS_DATA_FIELD) {
	OSD_TRACE(OSD_TRACE_T10,1,"Invalid flags of data_ofs key_spf descriptor 0x%x.\n",
		  (int)desc_p->flags);
	return;
    }
    *invalid_data_field_offset_p = NTOHS(desc_p->field_offset);
}

static inline void osd_sense_add_attr_id(
    osd_sense_t *sense_p,
    uint32_t page_num, uint32_t attr_num)
{
    osd_sense_osd_attr_desc_t desc;
    int desc_len=sizeof(osd_sense_osd_attr_desc_t);
    
    memset(&desc, 0, desc_len);

    desc.key=OSD_SENSE_DESC_OSD_ATTR;
    desc.add_len=desc_len-2;
    desc.page_num =HTONL(page_num);
    desc.attr_num =HTONL(attr_num);

    osd_sense_add_desc(sense_p, &desc);
}


static inline void osd_sense_desc_extract_attr_id(
    const void *descriptor_p,
    uint32_t *page_num_p, uint32_t *attr_num_p)
{
    const osd_sense_osd_attr_desc_t *desc_p = (osd_sense_osd_attr_desc_t *)descriptor_p;
    int desc_len=sizeof(osd_sense_osd_attr_desc_t);

    osd_assert(desc_p);
    osd_assert(desc_p->key == OSD_SENSE_DESC_OSD_ATTR);
    if (desc_p->add_len+2!=desc_len) {
	OSD_TRACE(OSD_TRACE_T10,1,"Invalid length of attr_id descriptor %d. should be %d\n",
		  (int)desc_p->add_len+2,
		  desc_len);
	return;
    }
    
    *page_num_p = NTOHL(desc_p->page_num);
    *attr_num_p = NTOHL(desc_p->attr_num);
}

static inline uint16_t osd_cdb_field_to_offset(osd_bad_cdb_field_t bad_field) 
{
    switch (bad_field) {
    case OSD_BAD_SERVICE_ACTION: return 8;

    case OSD_BAD_PARTITION_ID: return 16;

    case OSD_BAD_OBJECT_ID: return 24;

    case OSD_BAD_LENGTH: return 36;

    case OSD_BAD_STARTING_BYTE: return 44;

    case OSD_BAD_CREDENTIAL_EXIRATION_TIME: return 80+4;
    case OSD_BAD_POL_ACCESS_TAG: return 80+56;
    case OSD_BAD_CREATION_TIME: return 80+42;
    case OSD_BAD_CAP_KEY_VER: return 80+1;

    case OSD_BAD_CAPABILITY: return 80;

    case OSD_BAD_INTEGRITY_CHECK_VALUE: return 160;

    case OSD_BAD_LIST_ID: return 32;

    case OSD_BAD_CDB_KEY_VER: return 24;

    case OSD_BAD_CDB_ATTR_PARAMS: return 52;

    case OSD_UNKNOWN_FIELD: return 0;
    }
    OSD_ERROR("unsupported bad field reported.\n");
    return 0;
}

static inline osd_bad_cdb_field_t osd_cdb_offset_to_field(uint16_t service_action,
							  uint16_t ofs) 
{
    switch (ofs) {
    case 8: return OSD_BAD_SERVICE_ACTION;

    case 16: return OSD_BAD_PARTITION_ID;

    case 32: return OSD_BAD_LIST_ID;

    case 24: 
	if (OSD_CMD_SET_KEY==service_action) {
	    return OSD_BAD_CDB_KEY_VER;
	} else {
	    return OSD_BAD_OBJECT_ID;
	}
	break;

    case 36: return OSD_BAD_LENGTH;

    case 44: return OSD_BAD_STARTING_BYTE;

    case 80+4: return OSD_BAD_CREDENTIAL_EXIRATION_TIME;
    case 80+56: return OSD_BAD_POL_ACCESS_TAG;
    case 80+42: return OSD_BAD_CREATION_TIME;
    case 80+1: return OSD_BAD_CAP_KEY_VER;

    case 160: return OSD_BAD_INTEGRITY_CHECK_VALUE;

    default: 
	if ( (ofs>=80) && (ofs<160) ) return OSD_BAD_CAPABILITY;
	if ( (ofs>=52) && (ofs<=80) ) return OSD_BAD_CDB_ATTR_PARAMS;
	//else...
	return OSD_UNKNOWN_FIELD;;
    }
}

static inline void osd_sense_add_debug_desc(osd_sense_t *sense_p, 
					    uint8_t debug_len,
					    const uint8_t *debug_data)
{

}

static inline void osd_sense_encap( osd_sense_t *sense_p, //output
				    const osd_result_t *result_p)
{
    osd_assert(sense_p);
    osd_assert(result_p);
    
    osd_sense_encap_hdr (sense_p, result_p->rc);

    if (OSD_RC_GOOD==result_p->rc) return;

    osd_sense_add_osd_error_id(sense_p, 
			       result_p->command_sense.not_initiated_cmd_funcs,
			       result_p->command_sense.completed_cmd_funcs,
			       result_p->command_sense.partition_id,
			       result_p->command_sense.object_id);

    if (result_p->debug_sense_len) {
	osd_sense_add_debug_desc(sense_p, 
				 result_p->debug_sense_len,
				 (const uint8_t*)result_p->debug_sense);
    }

    switch (result_p->rc) {
    case OSD_RC_INVALID_CDB: {
	uint16_t ofs = result_p->cdb_field_sense.bad_field_offset;
	if (0==ofs) {
	    ofs=osd_cdb_field_to_offset(result_p->cdb_field_sense.bad_field);
	}
	osd_sense_add_cdb_field_offset(sense_p, ofs);
	break;
    }

    case OSD_RC_INVALID_DATA_OUT:
	osd_sense_add_data_field_offset(
	    sense_p, result_p->data_field_sense.bad_field_offset);
	break;
	
    case OSD_RC_QUOTA_ERROR:
	osd_sense_add_attr_id(sense_p, 
			      result_p->quota_sense.page_num,
			      result_p->quota_sense.attr_num);
	break;

    case OSD_RC_READ_PAST_END_OF_OBJECT:
	osd_sense_add_num_bytes_read(
	    sense_p, result_p->read_past_end_sense.actually_read_bytes);
	break;

    default:
	//no more sense to insert
	break;
    }

    osd_trace(OSD_TRACE_T10,2,"sense bytes:\n");
    osd_trace_buf(OSD_TRACE_T10,2,
		  sense_p->sense_buf,sense_p->sense_len);

}
				    
static inline osd_bool_t osd_sense_decap( const osd_sense_t *sense_p,
					  uint16_t service_action,
					  osd_result_t *result_p)
{  
    char desc_a[OSD_SENSE_MAX_LEN];
    int desc_len;

    osd_assert(sense_p);
    osd_assert(result_p);
    osd_assert(sense_p->sense_len <= sizeof(sense_p->sense_buf) );

    memset(result_p, 0, sizeof(osd_result_t));

    if (SCSI_GOOD != sense_p->status) {
	osd_trace(OSD_TRACE_T10,2,"sense bytes:\n");
	osd_trace_buf(OSD_TRACE_T10,2,
		      sense_p->sense_buf,sense_p->sense_len);
    }

    //try extracting osd error descriptor for any type of error
    if (!osd_sense_decap_hdr (sense_p, &result_p->rc)) return OSD_FALSE;

    if (osd_sense_extract_desc_by_key(sense_p, 
				      OSD_SENSE_DESC_OSD_ERROR,
				      desc_a,
				      &desc_len)) {
	//copy params from descriptor into result struct
	osd_sense_desc_extract_osd_error_id(
	    desc_a,
	    &result_p->command_sense.not_initiated_cmd_funcs,
	    &result_p->command_sense.completed_cmd_funcs,
	    &result_p->command_sense.partition_id,
	    &result_p->command_sense.object_id);
    }
    result_p->command_sense.service_action = service_action;
    
    //tbd!!! try extracting debug data for any error
    
    switch (result_p->rc) {
    case OSD_RC_INVALID_CDB: {
	//try to extract field offset
	if (osd_sense_extract_desc_by_key(sense_p, 
					  OSD_SENSE_DESC_KEY_SPF,
					  desc_a,
					  &desc_len)) {
	    //copy params from descriptor into result struct
	    osd_sense_desc_extract_cdb_field_offset(
		desc_a,
		&result_p->cdb_field_sense.bad_field_offset);

	    result_p->cdb_field_sense.bad_field = 
		osd_cdb_offset_to_field(
		    result_p->command_sense.service_action,
		    result_p->cdb_field_sense.bad_field_offset);
	    OSD_TRACE( OSD_TRACE_T10,1, 
		       "INVALID FIELD IN CDB at ofs %d (%s)\n",
		       result_p->cdb_field_sense.bad_field_offset,
		       osd_bad_field_name(result_p->cdb_field_sense.bad_field));
	}
	break;
    }

    case OSD_RC_INVALID_DATA_OUT:
	//try to extract field offset
	if (osd_sense_extract_desc_by_key(sense_p, 
					  OSD_SENSE_DESC_KEY_SPF,
					  desc_a,
					  &desc_len)) {
	    //copy params from descriptor into result struct
	    osd_sense_desc_extract_data_field_offset(
		desc_a,
		&result_p->data_field_sense.bad_field_offset);
	    OSD_TRACE( OSD_TRACE_T10,1, 
		       "INVALID FIELD IN PARAMETER DATA at ofs %d\n",
		       result_p->data_field_sense.bad_field_offset);
	}
	break;

    case OSD_RC_QUOTA_ERROR:
	//try to extract relevant quota attribute
	if (osd_sense_extract_desc_by_key(sense_p, 
					  OSD_SENSE_DESC_OSD_ATTR,
					  desc_a,
					  &desc_len)) {
	    //copy params from descriptor into result struct
	    osd_sense_desc_extract_attr_id(
		desc_a,
		&result_p->quota_sense.page_num,
		&result_p->quota_sense.attr_num);
	    OSD_TRACE( OSD_TRACE_T10,1, 
		       "exceeded quota defined by pg 0x%x attr 0x%x\n",
		       result_p->quota_sense.page_num,
		       result_p->quota_sense.attr_num);
	}
	break;

    case OSD_RC_READ_PAST_END_OF_OBJECT:
	//try to extract number of bytes actually read
	if (osd_sense_extract_desc_by_key(sense_p, 
					  OSD_SENSE_DESC_CMD_INFO,
					  desc_a,
					  &desc_len)) {
	    //copy params from descriptor into result struct
	    osd_sense_desc_get_num_bytes_read(
		desc_a,
		&result_p->read_past_end_sense.actually_read_bytes);
	    OSD_TRACE( OSD_TRACE_T10,1, 
		       "READ_PAST_END. bytes actually read: %lld\n",
		       result_p->read_past_end_sense.actually_read_bytes);
	}

	break;

    default:
	//no more sense to insert
	break;
    } 

    return OSD_TRUE;
}

#endif
