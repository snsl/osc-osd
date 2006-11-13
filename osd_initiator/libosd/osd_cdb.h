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

#ifndef OSD_CDB_H
#define OSD_CDB_H


// some common osd definitions are dound in osd_formats.h
// this is because they are used by auto-generated code that
// uses only osd_formats.h

#include "osd.h"
#include "osd_formats.h"
#include "osd_sense.h"

/************************************
 * File:  osd_cdb.h
 * constants, structs and functions for reading and writing 
 * command-independent parts of cdb.
 */

/*HOW TO USE THIS TO HANDLE A SCSI OSD COMMAND:
 *
 * in order to create an osd command one should:
 * 1. allocate a buffer of size OSD_CDB_LEN bytes to hold cdb.
 * 2. for any command:
 *    call osd_encap_cdb_hdr() - to initialize entire cdb and fill cdb header
 * 3. for the specific command (e.g. read):
 *    call encap function (look at osd_cdb_cmd.h for the right function). 
 *     (e.g. osd_encap_cdb_cmd_read())
 *     - this function should fill into the cdb all command-specific parts
 * 4. if further parts of cdb used (attr/integrity/capability)
 *    find the apropriate osd_encap_cdb... function in osd_cdb.h and use it.
 * 5. if transfering attributes lists in data-in/data-out scsi buffers
 *    get the appropriate buffer and use appropriate init and iterate
 *    functions defined in osd_attr.h
 *
 */

/******************************************************
 * osd service action codes:
 */

typedef union {
    OSD_CDB_HDR_t hdr;
    OSD_CDB_CMD_ON_SOMETHING_t cmd;
    OSD_CDB_ATTR_LIST_t alist;
    OSD_CDB_ATTR_PAGE_t apage;
    OSD_CDB_SEC_PARAMS_t sec;
} PACKED OSD_CDB_PARTS_t;

#define OSD_CDB_LEN (sizeof(OSD_CDB_PARTS_t))
#define OSD_NON_OSD_CDB_LEN  6

#define ATTR_FMT_MASK (3<<4)
#define ATTR_FMT_PAGE (2<<4)
#define ATTR_FMT_LIST (3<<4)

#define OSD_TIMESTAMP_ATTR_NORMAL 0
#define OSD_TIMESTAMP_ATTR_BYPASS 0x7f
#define OSD_CDB_USE_TIMESTAMP_CTL 0x80
#define OSD_CDB_TIMESTAMP_NO_CTL 0 //don't use the bypass value from cdb
#define OSD_CDB_TIMESTAMP_NORMAL     \
 (OSD_CDB_USE_TIMESTAMP_CTL|OSD_TIMESTAMP_ATTR_NORMAL)
#define OSD_CDB_TIMESTAMP_BYPASS     \
 (OSD_CDB_USE_TIMESTAMP_CTL| OSD_TIMESTAMP_ATTR_BYPASS)

/*COMMON FUNCTIONS */


/**********************************
 * functions for reading/writing cdb parts
 * each osd_decap_cdb_... function transfers values from cdb to 
 * output params for use by osd application.
 * each encap function transfers values from input params to cdb
 * encap/decap functions also handle NtoH/HtoN byte-order conversions where needed
 */

/**
 *  Function:  osd_encap_cdb_hdr
 *
 *  Purpose:   initialize cdb and fill it's header (1st 12 bytes)
 *
 *  Arguments: @cdb - non NULL ptr to preallocated cdb buffer to be filled
 *             src_opcode,        - value for opcode field in cdb
 *             src_control,       - value for control field in cdb
 *             src_tot_cdb_len    - total length of cdb.
 *             src_service_action - value for service_action field in cdb
 *
 *  Returns:   void
 *
 **/
static inline void osd_encap_cdb_hdr(uint8_t *cdb,
				     uint8_t src_opcode,
				     uint8_t src_control,
				     uint8_t src_tot_cdb_len, 
				     uint16_t src_service_action,
				     uint8_t src_timestamp_ctl
    );


/**
 *  Function:  osd_decap_cdb_hdr
 *
 *  Purpose:   interpret cdb header
 *
 *  Arguments: @cdb - non NULL ptr to cdb buffer to be interpreted
 *             pointers to params to be filled with interpreted values
 *             @dst_opcode,       
 *             @dst_control,      
 *             @dst_tot_cdb_len   
 *             @dst_service_action
 *
 *  Returns:   OSD_TRUE - upon success
 *             OSD_FALSE  - if invalid value was found in cdb
 *
 **/
static inline osd_bool_t osd_decap_cdb_hdr(const uint8_t *cdb, osd_result_t *result_p, 
				     uint8_t *dst_opcode, 
				     uint8_t *dst_control, 
				     uint8_t *dst_tot_cdb_len, 
				     uint16_t *dst_service_action,
				     uint8_t  *dst_timestamp_ctl);

/**
 *  Function:  osd_decap_cdb_attr_format
 *  Purpose:   interpret GET/SET CDBFMT field in 2nd options byte of cdb
 *  Arguments: @cdb - non NULL ptr to cdb buffer to be interpreted
 *             @dst_attr_format - filled with interpreted format
 *
 *  Returns:   OSD_TRUE - upon success
 *             OSD_FALSE  - if invalid value was found in cdb
 **/
static inline osd_bool_t osd_decap_cdb_attr_format(const uint8_t *cdb, osd_result_t *result_p,
					     uint8_t *dst_attr_format);

/**
 *  Function:  osd_encap_cdb_attr_list
 *
 *  Purpose:   fill cdb bytes 80..107 in attr-list format
 *
 *  Arguments: @cdb - non NULL ptr to preallocated cdb buffer to be filled
 *
 *             src_get_list_length - length of attributes list
 *               to get in data-out buffer.
 *               0 value in this argument means no attribute get action
 *               is required.(other get related args fill be ignored)
 *
 *             src_get_list_dout_offset - offset of get list in data-out
 *               ignored if src_get_list_length=0.
 *
 *             src_get_allocation_length - 
 *               max length in data-in of retrived attr-list.
 *               ignored if src_get_list_length=0.
 *
 *             src_retrieved_attr_din_offset -
 *               offset of retrived attr-list in data-in buffer.
 *               ignored if src_get_list_length=0.
 *
 *             src_set_list_length - length of attr-set-list
 *               in data-out buffer.(0 marks no attr-set action required)
 *
 *             src_set_list_dout_offset -
 *               offset of attr-set-list in data-out buffer.
 *               ignored if src_set_list_length=0
 *
 *   Returns:  void
 *
 **/
static inline void osd_encap_cdb_attr_list(
    uint8_t *cdb,
    uint32_t src_get_list_length, 
    uint64_t src_get_list_dout_offset, 
    uint32_t src_get_allocation_length, 
    uint64_t src_retrieved_attr_din_offset, 
    uint32_t src_set_list_length, 
    uint64_t src_set_list_dout_offset );


/**
 *  Function:  osd_decap_cdb_attr_list
 *  Purpose:   interpret offsets 80..107 of cdb, assuming
 *             they are in list format.
 *
 *  Arguments: @cdb - non NULL ptr to cdb buffer to be interpreted
 *            pointers to params to be filled with interpreted values:
 *
 *             @dst_get_list_length - length of attributes list
 *               to get in data-out buffer.
 *               0 value in this argument means no attribute get action
 *               is required. and other get related params should be ignored.
 *
 *             @dst_get_list_dout_offset - offset of get list in data-out
 *
 *             @dst_get_allocation_length - 
 *               max length in data-in of retrived attr-list
 *
 *             @dst_retrieved_attr_din_offset -
 *               offset of retrived attr-list in data-in buffer.
 *
 *             @dst_set_list_length - length of attr-set-list
 *               in data-out buffer.(0 marks no attr-set action required)
 *
 *             @dst_set_list_dout_offset -
 *               offset of attr-set-list in data-out buffer.
 *
 *  Returns:   OSD_TRUE - upon success
 *             OSD_FALSE  - if invalid value was found in cdb
 **/
static inline osd_bool_t osd_decap_cdb_attr_list(
    const uint8_t *cdb, osd_result_t *result_p,
    uint32_t *dst_get_list_length, 
    uint64_t *dst_get_list_dout_offset, 
    uint32_t *dst_get_allocation_length, 
    uint64_t *dst_retrieved_attr_din_offset, 
    uint32_t *dst_set_list_length, 
    uint64_t *dst_set_list_dout_offset);


/********************************* implementations******************/

#define OSD_CDB_ADDLEN_DELTA 8

static inline osd_bool_t osd_decap_cdb_hdr(const uint8_t *cdb, osd_result_t *result_p, 
				     uint8_t *dst_opcode, 
				     uint8_t *dst_control, 
				     uint8_t *dst_tot_cdb_len, 
				     uint16_t *dst_service_action,
				     uint8_t  *dst_timestamp_ctl) {
    const OSD_CDB_HDR_t *phdr=(const OSD_CDB_HDR_t *)cdb;
    uint8_t op1,op2,add_len;

    osd_assert(cdb);
    osd_assert(dst_opcode);
    osd_assert(dst_control);
    osd_assert(dst_tot_cdb_len);
    osd_assert(dst_service_action);
    osd_assert(dst_timestamp_ctl);

    GENERIC_DECAP_CDB_HDR(phdr,
			  dst_opcode,
			  dst_control,
			  &add_len,
			  dst_service_action,
			  &op1,&op2,
			  dst_timestamp_ctl);

    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
	PRINT("CDB HEADER CONTENTS:\n");
	GENERIC_PRINT_CDB_HDR(phdr);
    }
    *dst_tot_cdb_len= add_len+OSD_CDB_ADDLEN_DELTA;
    switch (*dst_opcode) {
	/*valid opcodes:*/
    case 0x7F: /*osd command*/ {
	    if (*dst_tot_cdb_len!=OSD_CDB_LEN) {
		OSD_TRACE(OSD_TRACE_T10,1,
			  "osd commands cdb length:%uc <>%d bytes\n",
			  *dst_tot_cdb_len,OSD_CDB_LEN);
	    }
	    break; 
	}
    default:
	OSD_TRACE(OSD_TRACE_T10,1,
		  "scsi opcode: %uc unsupported by osd\n",*dst_opcode);
	result_p->rc = OSD_RC_INVALID_OPCODE;
	return OSD_FALSE;
    }
    return OSD_TRUE;
}


/*osd_encap_cdb_hdr:
should be the first part written to cdb since it also initializes all other parts of cdb
in case they are not used*/
static inline void osd_encap_cdb_hdr(uint8_t *cdb,
				     uint8_t src_opcode,
				     uint8_t src_control,
				     uint8_t src_tot_cdb_len, 
				     uint16_t src_service_action,
				     uint8_t src_timestamp_ctl
    ) {
    OSD_CDB_HDR_t *phdr=(OSD_CDB_HDR_t *)cdb;
    
    osd_assert(cdb);
    
    switch (src_opcode) {
	/*valid opcodes:*/
    case 0x7F: /*osd command*/ {
	if (src_tot_cdb_len!=OSD_CDB_LEN) {
	    OSD_TRACE(OSD_TRACE_T10,1,
		      "osd commands cdb length:%uc <>%d bytes\n",
		      src_tot_cdb_len,OSD_CDB_LEN);
	}
	break;
    }
    default:
	OSD_TRACE(OSD_TRACE_T10,1,
		  "scsi opcode: %uc unsupported by osd\n",src_opcode);
    }
    
    /*initialize entire cdb with zeros*/
    memset(cdb,0,src_tot_cdb_len);

    GENERIC_ENCAP_CDB_HDR(phdr,
			  src_opcode,
			  src_control,
			  src_tot_cdb_len-OSD_CDB_ADDLEN_DELTA,
			  src_service_action,
			  0 /*options_byte1*/,
			  ATTR_FMT_PAGE /*options_byte2*/,
			  src_timestamp_ctl);
    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
	PRINT("CDB HEADER CONTENTS:\n");
	GENERIC_PRINT_CDB_HDR(phdr);
    }

    /*if we got so far, all should be well*/
}

static inline osd_bool_t osd_decap_cdb_attr_format(const uint8_t *cdb, osd_result_t *result_p,
					     uint8_t *dst_attr_format) {
    const OSD_CDB_HDR_t *phdr;
    osd_assert(cdb);
    osd_assert(dst_attr_format);
    phdr=(const OSD_CDB_HDR_t *)cdb;

    *dst_attr_format=(phdr->options_byte2)&ATTR_FMT_MASK;
    if ( ((*dst_attr_format)!=ATTR_FMT_PAGE) &&
	 ((*dst_attr_format)!=ATTR_FMT_LIST) ) {
	OSD_TRACE(OSD_TRACE_T10,1,
		  "bad attr cdb format value:0x%x!={0x%x,0x%x}\n",
		  (int)*dst_attr_format,
		  (int)ATTR_FMT_PAGE,(int)ATTR_FMT_LIST);
	result_p->rc=OSD_RC_INVALID_CDB;
	result_p->cdb_field_sense.bad_field_offset = (&(phdr->options_byte2))-cdb;
	return OSD_FALSE;
    } else {
	return OSD_TRUE;
    }
}

static inline osd_bool_t osd_decap_cdb_attr_list(
    const uint8_t *cdb, osd_result_t *result_p,
    uint32_t *dst_get_list_length, 
    uint64_t *dst_get_list_dout_offset, 
    uint32_t *dst_get_allocation_length, 
    uint64_t *dst_retrieved_attr_din_offset, 
    uint32_t *dst_set_list_length, 
    uint64_t *dst_set_list_dout_offset) 
{		   
    const OSD_CDB_ATTR_LIST_t *plist = (const OSD_CDB_ATTR_LIST_t *)cdb;
    uint8_t attr_format;
    
    osd_decap_cdb_attr_format(cdb,result_p,&attr_format);

    osd_assert(attr_format==ATTR_FMT_LIST);

    GENERIC_DECAP_CDB_ATTR_LIST(
	plist,
	dst_get_list_length, 
	dst_get_list_dout_offset, 
	dst_get_allocation_length, 
	dst_retrieved_attr_din_offset, 
	dst_set_list_length, 
	dst_set_list_dout_offset);
    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
	PRINT("CDB ATTR LIST PARAMS:\n");
	GENERIC_PRINT_CDB_ATTR_LIST(plist);
    }

    if (0==*dst_get_list_length) {
	*dst_get_list_dout_offset=OFFSET_IGNORE;
	*dst_get_allocation_length=0;
	*dst_retrieved_attr_din_offset=OFFSET_IGNORE;
    }
    if (0==*dst_set_list_length) {
	*dst_set_list_dout_offset=OFFSET_IGNORE;
    }
 
    return OSD_TRUE;
}

/*also updates second option byte in cdb to reflect format*/
static inline void osd_encap_cdb_attr_list(
    uint8_t *cdb,
    uint32_t src_get_list_length, 
    uint64_t src_get_list_dout_offset, 
    uint32_t src_get_allocation_length, 
    uint64_t src_retrieved_attr_din_offset, 
    uint32_t src_set_list_length, 
    uint64_t src_set_list_dout_offset ) 
{
    OSD_CDB_ATTR_LIST_t *plist = (OSD_CDB_ATTR_LIST_t *)cdb;
    OSD_CDB_HDR_t *phdr=(OSD_CDB_HDR_t*)cdb;

    osd_assert(cdb);
    SET_MASKED_VAL((phdr->options_byte2),ATTR_FMT_MASK,ATTR_FMT_LIST);

    if (0==src_get_list_length) {
	src_get_list_dout_offset=OFFSET_IGNORE;
	src_get_allocation_length=0;
	src_retrieved_attr_din_offset=OFFSET_IGNORE;
    }
    if (0==src_set_list_length) {
	src_set_list_dout_offset=OFFSET_IGNORE;
    }

    GENERIC_ENCAP_CDB_ATTR_LIST( plist,
				 src_get_list_length, 
				 src_get_list_dout_offset, 
				 src_get_allocation_length, 
				 src_retrieved_attr_din_offset, 
				 src_set_list_length, 
				 src_set_list_dout_offset);
    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
	PRINT("CDB ATTR LIST PARAMS:\n");
	GENERIC_PRINT_CDB_ATTR_LIST(plist);
    }
}

/*also updates second option byte in cdb to reflect format*/
static inline void osd_encap_cdb_attr_page(
    uint8_t *cdb,
    uint32_desc_t src_get_page_num, 
    uint32_desc_t src_get_allocation_length, 
    offset32_desc_t src_retrieved_attr_din_offset, 
    uint32_desc_t src_set_page_num, 
    uint32_desc_t src_set_attr_num, 
    uint32_desc_t src_set_attr_length, 
    offset32_desc_t src_set_attr_dout_offset )
{
    OSD_CDB_ATTR_PAGE_t *p_cdb_attr_page =
                    (OSD_CDB_ATTR_PAGE_t*)cdb;
    OSD_CDB_HDR_t *phdr=(OSD_CDB_HDR_t*)cdb;

    osd_assert(cdb);
    SET_MASKED_VAL((phdr->options_byte2),ATTR_FMT_MASK,ATTR_FMT_PAGE);

    if (0==src_get_page_num) {
	src_get_allocation_length=0;
	src_retrieved_attr_din_offset=OFFSET_IGNORE;
    }
    if (0==src_set_page_num) {
	src_set_attr_num=0;
	src_set_attr_length=0;
	src_set_attr_dout_offset=OFFSET_IGNORE;
    }

    GENERIC_ENCAP_CDB_ATTR_PAGE(
        p_cdb_attr_page,
        src_get_page_num,
        src_get_allocation_length,
        src_retrieved_attr_din_offset,
        src_set_page_num,
        src_set_attr_num,
        src_set_attr_length,
        src_set_attr_dout_offset );
    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
        PRINT("CDB_ATTR_PAGE PARAMS:\n");
        GENERIC_PRINT_CDB_ATTR_PAGE(p_cdb_attr_page);
    }
}

static inline osd_bool_t osd_decap_cdb_attr_page(
    const uint8_t *cdb, osd_result_t *result_p,
    uint32_desc_t *dst_get_page_num, 
    uint32_desc_t *dst_get_allocation_length, 
    offset32_desc_t *dst_retrieved_attr_din_offset, 
    uint32_desc_t *dst_set_page_num, 
    uint32_desc_t *dst_set_attr_num, 
    uint32_desc_t *dst_set_attr_length, 
    offset32_desc_t *dst_set_attr_dout_offset )
{
    const OSD_CDB_ATTR_PAGE_t *p_cdb_attr_page =
                    (const OSD_CDB_ATTR_PAGE_t*)cdb;
    uint8_t attr_format;

    osd_assert(cdb);
    osd_decap_cdb_attr_format(cdb,result_p, &attr_format);

    osd_assert(attr_format==ATTR_FMT_PAGE);

    GENERIC_DECAP_CDB_ATTR_PAGE(
        p_cdb_attr_page,
        dst_get_page_num,
        dst_get_allocation_length,
        dst_retrieved_attr_din_offset,
        dst_set_page_num,
        dst_set_attr_num,
        dst_set_attr_length,
        dst_set_attr_dout_offset );
    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
        PRINT("CDB_ATTR_PAGE PARAMS:\n");
        GENERIC_PRINT_CDB_ATTR_PAGE(p_cdb_attr_page);
    }
    if (0==*dst_get_page_num) {
	*dst_get_allocation_length=0;
	*dst_retrieved_attr_din_offset=OFFSET_IGNORE;
    }
    if (0==*dst_set_page_num) {
	*dst_set_attr_num=0;
	*dst_set_attr_length=0;
	*dst_set_attr_dout_offset=OFFSET_IGNORE;
    }
 
    return OSD_TRUE;
}

#endif
