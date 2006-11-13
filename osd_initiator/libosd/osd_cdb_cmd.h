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

#ifndef OSD_CDB_CMD_H
#define OSD_CDB_CMD_H

#include "osd_formats.h"
#include "osd_util.h"
/*******************************************
 *File: osd_cdb_cmd.h
 * definitions, structs and functions for reading/writing formats 
 * that are for specific commands.
 * see description of usage in osd_cdb.h
 */

/****************************************************** READ */


static inline void osd_encap_cdb_cmd_read(uint8_t *cdb, 
					  uint64_t src_partition_id, 
					  uint64_t src_obj_id, 
					  uint64_t src_length, 
					  uint64_t src_starting_byte_ofs ) {
    OSD_CDB_CMD_READWRITE_t *pread=
	(OSD_CDB_CMD_READWRITE_t *)cdb;
    osd_assert(cdb);

    GENERIC_ENCAP_CDB_CMD_READWRITE(pread,
				    src_partition_id,
				    src_obj_id,
				    src_length,
				    src_starting_byte_ofs);
    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
	PRINT("CDB READ/WRITE PARAMS:\n");
	GENERIC_PRINT_CDB_CMD_READWRITE(pread);
    }
}


static inline void osd_encap_cdb_cmd_inquiry(uint8_t *cdb, 
                                             uint64_t src_length,
                                             uint8_t page) {

    uint16_t alloc_len = src_length;
    
    osd_assert(cdb);
    cdb[0] = 0x12;
    if (page != 0) {
        cdb[1] = 1;
        cdb[2] = page;
    }
    cdb[3] = (uint8_t)(alloc_len >> 8);
    cdb[4] = (uint8_t)(alloc_len & ((uint16_t)0x00ff));
}


static inline osd_bool_t osd_decap_cdb_cmd_read(const uint8_t *cdb, osd_result_t *result_p, 
                                                uint64_t *dst_partition_id, 
                                                uint64_t *dst_obj_id, 
                                                uint64_t *dst_length, 
                                                uint64_t *dst_starting_byte_ofs) {
    const OSD_CDB_CMD_READWRITE_t *pread=
	(const OSD_CDB_CMD_READWRITE_t *)cdb;
    osd_assert(cdb);

    GENERIC_DECAP_CDB_CMD_READWRITE(pread,
				    dst_partition_id,
				    dst_obj_id,
				    dst_length,
				    dst_starting_byte_ofs);
    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
	PRINT("CDB READ/WRITE PARAMS:\n");
	GENERIC_PRINT_CDB_CMD_READWRITE(pread);
    }

    return OSD_TRUE;
}

/****************************************************** WRITE */


static inline void osd_encap_cdb_cmd_write(uint8_t *cdb, 
					   uint64_t src_partition_id, 
					   uint64_t src_obj_id, 
					   uint64_t src_length, 
					   uint64_t src_starting_byte_ofs ) {
    OSD_CDB_CMD_READWRITE_t *pwrite=
	(OSD_CDB_CMD_READWRITE_t *)cdb;
    osd_assert(cdb);

    GENERIC_ENCAP_CDB_CMD_READWRITE(pwrite,
				    src_partition_id,
				    src_obj_id,
				    src_length,
				    src_starting_byte_ofs);
    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
	PRINT("CDB READ/WRITE PARAMS:\n");
	GENERIC_PRINT_CDB_CMD_READWRITE(pwrite);
    }
}

static inline osd_bool_t osd_decap_cdb_cmd_write(const uint8_t *cdb, osd_result_t *result_p, 
					  uint64_t *dst_partition_id, 
					  uint64_t *dst_obj_id, 
					  uint64_t *dst_length, 
					  uint64_t *dst_starting_byte_ofs) {
    const OSD_CDB_CMD_READWRITE_t *pwrite=
	(const OSD_CDB_CMD_READWRITE_t *)cdb;
    osd_assert(cdb);

    GENERIC_DECAP_CDB_CMD_READWRITE(pwrite,
				    dst_partition_id,
				    dst_obj_id,
				    dst_length,
				    dst_starting_byte_ofs);
    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
	PRINT("CDB READ/WRITE PARAMS:\n");
	GENERIC_PRINT_CDB_CMD_READWRITE(pwrite);
    }

    return OSD_TRUE;
}

/****************************************************** APPEND */
static inline void osd_encap_cdb_cmd_append(
    uint8_t *cdb,
    uint64_t src_partition_id, 
    uint64_t src_obj_id, 
    uint64_t src_length )
{
    OSD_CDB_CMD_APPEND_t *p_cdb_cmd_append =
	(OSD_CDB_CMD_APPEND_t*)cdb;
    osd_assert(cdb);
    GENERIC_ENCAP_CDB_CMD_APPEND(
        p_cdb_cmd_append,
        src_partition_id,
        src_obj_id,
        src_length );
    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
        PRINT("CDB_CMD_APPEND PARAMS:\n");
        GENERIC_PRINT_CDB_CMD_APPEND(p_cdb_cmd_append);
    }
}


static inline osd_bool_t osd_decap_cdb_cmd_append(
    const uint8_t *cdb, osd_result_t *result_p,
    uint64_t *dst_partition_id, 
    uint64_t *dst_obj_id, 
    uint64_t *dst_length )
{
    const OSD_CDB_CMD_APPEND_t *p_cdb_cmd_append =
                    (const OSD_CDB_CMD_APPEND_t*)cdb;
    osd_assert(cdb);
    GENERIC_DECAP_CDB_CMD_APPEND(
        p_cdb_cmd_append,
        dst_partition_id,
        dst_obj_id,
        dst_length );
    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
        PRINT("CDB_CMD_APPEND PARAMS:\n");
        GENERIC_PRINT_CDB_CMD_APPEND(p_cdb_cmd_append);
    }
    return OSD_TRUE;
}

/****************************************************** ATTR GET/SET */

static inline void osd_encap_cdb_cmd_getset_attr(
    uint8_t *cdb, 
    uint64_t src_partition_id, 
    uint64_t src_obj_id ) 
{
    OSD_CDB_CMD_ON_SOMETHING_t *pattr=
	(OSD_CDB_CMD_ON_SOMETHING_t *)cdb;
    osd_assert(cdb);
    
    GENERIC_ENCAP_CDB_CMD_ON_SOMETHING(pattr,
			       src_partition_id,
			       src_obj_id);
    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
	PRINT("CDB ATTR GET/SET OBJECT PARAMS:\n");
	GENERIC_PRINT_CDB_CMD_ON_SOMETHING(pattr);
    }
}

static inline osd_bool_t osd_decap_cdb_cmd_getset_attr(
    const uint8_t *cdb, osd_result_t *result_p, 
    uint64_t *dst_partition_id, 
    uint64_t *dst_obj_id )
{
    const OSD_CDB_CMD_ON_SOMETHING_t *pattr=
	(const OSD_CDB_CMD_ON_SOMETHING_t *)cdb;
    osd_assert(cdb);

    GENERIC_DECAP_CDB_CMD_ON_SOMETHING(pattr,
				    dst_partition_id,
				    dst_obj_id);
    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
	PRINT("CDB ATTR GET/SET OBJECT PARAMS:\n");
	GENERIC_PRINT_CDB_CMD_ON_SOMETHING(pattr);
    }

    return OSD_TRUE;
}

/****************************************************** CREATE */

static inline void osd_encap_cdb_cmd_create(
    uint8_t *cdb,
    uint64_t src_partition_id, 
    uint64_t src_requested_obj_id, 
    uint16_t src_num_of_objects )
{
    OSD_CDB_CMD_CREATE_t *p_cdb_cmd_create =
                    (OSD_CDB_CMD_CREATE_t*)cdb;
    osd_assert(cdb);
    GENERIC_ENCAP_CDB_CMD_CREATE(
        p_cdb_cmd_create,
        src_partition_id,
        src_requested_obj_id,
        src_num_of_objects );
    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
        PRINT("CDB_CMD_CREATE PARAMS:\n");
        GENERIC_PRINT_CDB_CMD_CREATE(p_cdb_cmd_create);
    }
}

static inline osd_bool_t osd_decap_cdb_cmd_create(
    const uint8_t *cdb, osd_result_t *result_p,
    uint64_t *dst_partition_id, 
    uint64_t *dst_requested_obj_id, 
    uint16_t *dst_num_of_objects )
{
    const OSD_CDB_CMD_CREATE_t *p_cdb_cmd_create =
                    (const OSD_CDB_CMD_CREATE_t*)cdb;
    osd_assert(cdb);
    GENERIC_DECAP_CDB_CMD_CREATE(
        p_cdb_cmd_create,
        dst_partition_id,
        dst_requested_obj_id,
        dst_num_of_objects );
    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
        PRINT("CDB_CMD_CREATE PARAMS:\n");
        GENERIC_PRINT_CDB_CMD_CREATE(p_cdb_cmd_create);
    }
    return OSD_TRUE;
}

/****************************************************** CREATE_AND_WRITE */

static inline void osd_encap_cdb_cmd_create_and_write(
    uint8_t *cdb,
    uint64_t src_partition_id, 
    uint64_t src_req_obj_id, 
    uint64_t src_length, 
    uint64_t src_starting_byte_ofs )
{
    OSD_CDB_CMD_CREATE_AND_WRITE_t *p_cdb_cmd_create_and_write =
                    (OSD_CDB_CMD_CREATE_AND_WRITE_t*)cdb;
    osd_assert(cdb);
    GENERIC_ENCAP_CDB_CMD_CREATE_AND_WRITE(
        p_cdb_cmd_create_and_write,
        src_partition_id,
        src_req_obj_id,
        src_length,
        src_starting_byte_ofs );
    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
        PRINT("CDB_CMD_CREATE_AND_WRITE PARAMS:\n");
        GENERIC_PRINT_CDB_CMD_CREATE_AND_WRITE(p_cdb_cmd_create_and_write);
    }
}

static inline osd_bool_t osd_decap_cdb_cmd_create_and_write(
    const uint8_t *cdb, osd_result_t *result_p,
    uint64_t *dst_partition_id, 
    uint64_t *dst_req_obj_id, 
    uint64_t *dst_length, 
    uint64_t *dst_starting_byte_ofs )
{
    const OSD_CDB_CMD_CREATE_AND_WRITE_t *p_cdb_cmd_create_and_write =
                    (const OSD_CDB_CMD_CREATE_AND_WRITE_t*)cdb;
    osd_assert(cdb);
    GENERIC_DECAP_CDB_CMD_CREATE_AND_WRITE(
        p_cdb_cmd_create_and_write,
        dst_partition_id,
        dst_req_obj_id,
        dst_length,
        dst_starting_byte_ofs );
    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
        PRINT("CDB_CMD_CREATE_AND_WRITE PARAMS:\n");
        GENERIC_PRINT_CDB_CMD_CREATE_AND_WRITE(p_cdb_cmd_create_and_write);
    }
    return OSD_TRUE;
}

/****************************************************** REMOVE */

static inline void osd_encap_cdb_cmd_remove(
    uint8_t *cdb,
    uint64_t src_partition_id, 
    uint64_t src_obj_id )
{
    OSD_CDB_CMD_ON_OBJECT_t *p_cdb_cmd_remove =
                    (OSD_CDB_CMD_ON_OBJECT_t*)cdb;
    osd_assert(cdb);
    GENERIC_ENCAP_CDB_CMD_ON_OBJECT(
        p_cdb_cmd_remove,
        src_partition_id,
        src_obj_id );
    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
        PRINT("CDB_CMD_REMOVE PARAMS:\n");
        GENERIC_PRINT_CDB_CMD_ON_OBJECT(p_cdb_cmd_remove);
    }
}

static inline osd_bool_t osd_decap_cdb_cmd_remove(
    const uint8_t *cdb, osd_result_t *result_p,
    uint64_t *dst_partition_id, 
    uint64_t *dst_obj_id )
{
    const OSD_CDB_CMD_ON_OBJECT_t *p_cdb_cmd_remove =
                    (const OSD_CDB_CMD_ON_OBJECT_t*)cdb;
    osd_assert(cdb);
    GENERIC_DECAP_CDB_CMD_ON_OBJECT(
        p_cdb_cmd_remove,
        dst_partition_id,
        dst_obj_id );
    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
        PRINT("CDB_CMD_REMOVE PARAMS:\n");
        GENERIC_PRINT_CDB_CMD_ON_OBJECT(p_cdb_cmd_remove);
    }
    return OSD_TRUE;
}

/****************************************************** FLUSH */

//flush scope values:
#define OSD_FLUSH_BOTH 0
#define OSD_FLUSH_ATTR 1
#define OSD_FLUSH_ALL_RECURSIVE 2
#define OSD_FLUSH_SCOPE_MASK 0x3

static inline void osd_encap_cdb_cmd_flush(
    uint8_t *cdb, 
    uint8_t  src_flush_scope,
    uint64_t src_partition_id, 
    uint64_t src_obj_id ) 
{
    OSD_CDB_CMD_ON_SOMETHING_t *pflush=
	(OSD_CDB_CMD_ON_SOMETHING_t *)cdb;
    OSD_CDB_HDR_t *phdr=(OSD_CDB_HDR_t *)cdb;
    osd_assert(cdb);
    
    SET_MASKED_VAL(phdr->options_byte1,OSD_FLUSH_SCOPE_MASK,src_flush_scope);

    GENERIC_ENCAP_CDB_CMD_ON_SOMETHING(pflush,
				       src_partition_id,
				       src_obj_id);
    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
	int ofs= (uint8_t*)(&phdr->options_byte1) - cdb;
	PRINT("CDB FLUSH OBJECT PARAMS:\n");
	UINT8_PRINT(ofs,"options byte 1",phdr->options_byte1);
	GENERIC_PRINT_CDB_CMD_ON_SOMETHING(pflush);
    }
   switch (src_flush_scope) {
   case OSD_FLUSH_BOTH:
   case OSD_FLUSH_ATTR:
       break; //all options for partition id, object id are lagal
   case OSD_FLUSH_ALL_RECURSIVE:
       if (0!=src_obj_id) {
	   OSD_ERROR("flush with this scope can only refer to partition\n");
	   osd_assert(0);
	   return;
       }
       break;
   default:
       OSD_ERROR("illegal flush scope value\n");
       osd_assert(0);
       return;
   }
   
}

static inline osd_bool_t osd_decap_cdb_cmd_flush(
    const uint8_t *cdb, osd_result_t *result_p, 
    uint8_t  *dst_flush_scope,
    uint64_t *dst_partition_id, 
    uint64_t *dst_obj_id )
{
    const OSD_CDB_CMD_ON_SOMETHING_t *pflush=
	(const OSD_CDB_CMD_ON_SOMETHING_t *)cdb;
    const OSD_CDB_HDR_t *phdr=(const OSD_CDB_HDR_t *)cdb;
    osd_assert(cdb);

    GENERIC_DECAP_CDB_CMD_ON_SOMETHING(pflush,
				       dst_partition_id,
				       dst_obj_id);
    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
	PRINT("CDB FLUSH OBJECT PARAMS:\n");
	GENERIC_PRINT_CDB_CMD_ON_SOMETHING(pflush);
    }
    *dst_flush_scope=(phdr->options_byte1&OSD_FLUSH_SCOPE_MASK);
    switch (*dst_flush_scope) {
    case OSD_FLUSH_BOTH:
    case OSD_FLUSH_ATTR:
	break; //all options for partition id, object id are lagal
    case OSD_FLUSH_ALL_RECURSIVE:
	if (0!=*dst_obj_id) {
	    OSD_TRACE(OSD_TRACE_T10,1,
		      "flush with this scope can only refer to partition\n");
	    result_p->rc=OSD_RC_INVALID_CDB;
	    result_p->cdb_field_sense.bad_field_offset = 
		&(phdr->options_byte1) - cdb;
	    return OSD_FALSE;
	}
	break;
   default:
       OSD_TRACE(OSD_TRACE_T10,1,
		 "illegal flush scope value\n");
       result_p->rc=OSD_RC_INVALID_CDB;
       result_p->cdb_field_sense.bad_field_offset = 
	   &(phdr->options_byte1) - cdb;
       return OSD_FALSE;
    }

    return OSD_TRUE;
}

/****************************************************** CREATE_PARTITION */

static inline void osd_encap_cdb_cmd_create_partition(
    uint8_t *cdb,
    uint64_t src_req_partition_id )
{
    OSD_CDB_CMD_CREATE_PARTITION_t *p_cdb_cmd_create_partition =
                    (OSD_CDB_CMD_CREATE_PARTITION_t*)cdb;
    osd_assert(cdb);
    GENERIC_ENCAP_CDB_CMD_CREATE_PARTITION(
        p_cdb_cmd_create_partition,
        src_req_partition_id );
    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
        PRINT("CDB_CMD_CREATE_PARTITION PARAMS:\n");
        GENERIC_PRINT_CDB_CMD_CREATE_PARTITION(p_cdb_cmd_create_partition);
    }
}

static inline osd_bool_t osd_decap_cdb_cmd_create_partition(
    const uint8_t *cdb, osd_result_t *result_p,
    uint64_t *dst_req_partition_id )
{
    const OSD_CDB_CMD_CREATE_PARTITION_t *p_cdb_cmd_create_partition =
                    (const OSD_CDB_CMD_CREATE_PARTITION_t*)cdb;
    osd_assert(cdb);
    GENERIC_DECAP_CDB_CMD_CREATE_PARTITION(
        p_cdb_cmd_create_partition,
        dst_req_partition_id );
    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
        PRINT("CDB_CMD_CREATE_PARTITION PARAMS:\n");
        GENERIC_PRINT_CDB_CMD_CREATE_PARTITION(p_cdb_cmd_create_partition);
    }
    return OSD_TRUE;
}

/****************************************************** REMOVE_PARTITION */

static inline void osd_encap_cdb_cmd_remove_partition(
    uint8_t *cdb,
    uint64_t src_partition_id )
{
    OSD_CDB_CMD_ON_PARTITION_t *p_cdb_cmd_remove_partition =
                    (OSD_CDB_CMD_ON_PARTITION_t*)cdb;
    osd_assert(cdb);
    GENERIC_ENCAP_CDB_CMD_ON_PARTITION(
        p_cdb_cmd_remove_partition,
        src_partition_id );
    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
        PRINT("CDB_CMD_REMOVE_PARTITION PARAMS:\n");
        GENERIC_PRINT_CDB_CMD_ON_PARTITION(p_cdb_cmd_remove_partition);
    }
}

static inline osd_bool_t osd_decap_cdb_cmd_remove_partition(
    const uint8_t *cdb, osd_result_t *result_p,
    uint64_t *dst_partition_id )
{
    const OSD_CDB_CMD_ON_PARTITION_t *p_cdb_cmd_remove_partition =
                    (const OSD_CDB_CMD_ON_PARTITION_t*)cdb;
    osd_assert(cdb);
    GENERIC_DECAP_CDB_CMD_ON_PARTITION(
        p_cdb_cmd_remove_partition,
        dst_partition_id );
    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
        PRINT("CDB_CMD_REMOVE_PARTITION PARAMS:\n");
        GENERIC_PRINT_CDB_CMD_ON_PARTITION(p_cdb_cmd_remove_partition);
    }
    return OSD_TRUE;
}

/****************************************************** FORMAT */

static inline void osd_encap_cdb_cmd_format(
    uint8_t *cdb,
    uint64_t src_formatted_capacity )
{
    OSD_CDB_CMD_FORMAT_t *p_cdb_cmd_format =
                    (OSD_CDB_CMD_FORMAT_t*)cdb;
    osd_assert(cdb);
    GENERIC_ENCAP_CDB_CMD_FORMAT(
        p_cdb_cmd_format,
        src_formatted_capacity );
    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
        PRINT("CDB_CMD_FORMAT PARAMS:\n");
        GENERIC_PRINT_CDB_CMD_FORMAT(p_cdb_cmd_format);
    }
}

static inline osd_bool_t osd_decap_cdb_cmd_format(
    const uint8_t *cdb, osd_result_t *result_p,
    uint64_t *dst_formatted_capacity )
{
    const OSD_CDB_CMD_FORMAT_t *p_cdb_cmd_format =
                    (const OSD_CDB_CMD_FORMAT_t*)cdb;
    osd_assert(cdb);
    GENERIC_DECAP_CDB_CMD_FORMAT(
        p_cdb_cmd_format,
        dst_formatted_capacity );
    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
        PRINT("CDB_CMD_FORMAT PARAMS:\n");
        GENERIC_PRINT_CDB_CMD_FORMAT(p_cdb_cmd_format);
    }
    return OSD_TRUE;
}

/****************************************************** LIST */

#define OSD_LIST_ORDER_MASK 0x0F
#define OSD_LIST_ORDER_ASCENDING 0

#define LIST_LSTCHG_BIT_VAL 2
#define LIST_ROOT_BIT_VAL 1

static inline void osd_encap_cdb_cmd_list(
    uint8_t *cdb,
    uint64_t src_partition_id, 
    uint32_t src_list_identifier, 
    uint64_t src_allocation_length, 
    uint64_t src_initial_obj_id)
{
    OSD_CDB_CMD_LIST_t *p_cdb_cmd_list =
	(OSD_CDB_CMD_LIST_t*)cdb;
    OSD_CDB_HDR_t *phdr=(OSD_CDB_HDR_t *)cdb;
    uint8_t sort_order=OSD_LIST_ORDER_ASCENDING;
    osd_assert(cdb);
    SET_MASKED_VAL(phdr->options_byte2,
		   OSD_LIST_ORDER_MASK,sort_order);
    GENERIC_ENCAP_CDB_CMD_LIST(
        p_cdb_cmd_list,
        src_partition_id,
        src_list_identifier,
        src_allocation_length,
        src_initial_obj_id );
    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
	int ofs= (uint8_t*)(&phdr->options_byte2) - cdb;
        PRINT("CDB_CMD_LIST PARAMS:\n");
	UINT8_PRINT(ofs,"options byte 2",phdr->options_byte2);
        GENERIC_PRINT_CDB_CMD_LIST(p_cdb_cmd_list);
    }
}

static inline osd_bool_t osd_decap_cdb_cmd_list(
    const uint8_t *cdb, osd_result_t *result_p,
    uint64_t *dst_partition_id, 
    uint32_t *dst_list_identifier, /*zero for new list */
    uint64_t *dst_allocation_length, 
    uint64_t *dst_initial_obj_id /*continuation obj id if not new list */)
{
    const OSD_CDB_CMD_LIST_t *p_cdb_cmd_list =
	(const OSD_CDB_CMD_LIST_t*)cdb;
    OSD_CDB_HDR_t *phdr=(OSD_CDB_HDR_t *)cdb;
    uint8_t sort_order;

    osd_assert(cdb);
    GENERIC_DECAP_CDB_CMD_LIST(
        p_cdb_cmd_list,
        dst_partition_id,
        dst_list_identifier,
        dst_allocation_length,
        dst_initial_obj_id );
    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
        PRINT("CDB_CMD_LIST PARAMS:\n");
        GENERIC_PRINT_CDB_CMD_LIST(p_cdb_cmd_list);
    }
    sort_order=(phdr->options_byte2&OSD_LIST_ORDER_MASK);
    switch (sort_order) {
    case OSD_LIST_ORDER_ASCENDING: break;
    default:
	OSD_TRACE(OSD_TRACE_T10,1,
		  "illegal sort order value\n");
	result_p->rc=OSD_RC_INVALID_CDB;
	result_p->cdb_field_sense.bad_field_offset = 
	    &(phdr->options_byte2) - cdb;
	return OSD_FALSE;
    }
    return OSD_TRUE;
}

static inline void osd_encap_cmd_list_data_hdr(
    uint8_t *data_in_buf,
    uint64_t src_num_of_entries,
    //total num of entries that would have been retrieved if allocation length 
    // was infinite
    uint64_t src_continuation_obj_id, 
    //continuation tag is nonzero iff response was truncated because 
    //allocation length was smaller then full response
    uint32_t src_list_identifier, 
    //list identifier is used if src_continuation_obj_id<>0
    uint8_t src_flags 
    //possible flags: LIST_LSTCHG_BIT_VAL,LIST_ROOT_BIT_VAL
    )
{
    OSD_CMD_LIST_DATA_HDR_t *p_cmd_list_data_hdr =
                    (OSD_CMD_LIST_DATA_HDR_t*)data_in_buf;
    uint64_t add_len;
    add_len = ( std_sizeof(OSD_CMD_LIST_DATA_HDR_t) 
		+ (src_num_of_entries*sizeof(uint64_t)) 
		- 8 );
    
    osd_assert(data_in_buf);
    GENERIC_ENCAP_CMD_LIST_DATA_HDR(
        p_cmd_list_data_hdr,
        add_len,
        src_continuation_obj_id,
        src_list_identifier,
        src_flags );
    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
        PRINT("CMD_LIST_DATA_HDR PARAMS:\n");
        GENERIC_PRINT_CMD_LIST_DATA_HDR(p_cmd_list_data_hdr);
    }
}

static inline osd_bool_t osd_decap_cmd_list_data_hdr(
    const uint8_t *data_in_buf,
    uint64_t *dst_total_matches, 
    //total num of entries that would have been retrieved if allocation length 
    // was infinite
    uint64_t *dst_continuation_obj_id, 
    //continuation tag is nonzero iff response was truncated because 
    //allocation length was smaller then full response
    uint32_t *dst_list_identifier, 
    //list identifier is used if continuation tag<>0
    uint8_t *dst_flags
    //possible flags: LIST_LSTCHG_BIT_VAL,LIST_ROOT_BIT_VAL
    )
{
    const OSD_CMD_LIST_DATA_HDR_t *p_cmd_list_data_hdr =
                    (const OSD_CMD_LIST_DATA_HDR_t*)data_in_buf;
    uint64_t add_len;
    osd_assert(data_in_buf);
    GENERIC_DECAP_CMD_LIST_DATA_HDR(
        p_cmd_list_data_hdr,
        &add_len,
        dst_continuation_obj_id,
        dst_list_identifier,
        dst_flags );
    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
        PRINT("CMD_LIST_DATA_HDR PARAMS:\n");
        GENERIC_PRINT_CMD_LIST_DATA_HDR(p_cmd_list_data_hdr);
    }
    *dst_total_matches =
	(add_len+8-std_sizeof(OSD_CMD_LIST_DATA_HDR_t))/sizeof(uint64_t);
    if ( add_len != 
	 (std_sizeof(OSD_CMD_LIST_DATA_HDR_t)+
	  (*dst_total_matches*sizeof(uint64_t))-8) ) {
	OSD_TRACE(OSD_TRACE_T10,1,
		  "invalid additional length value in list response header\n");
	return OSD_FALSE;
    }
    return OSD_TRUE;
}

static inline uint64_t osd_calc_max_list_entries_in_len(uint64_t allocation_length)
{
    return 
	( (allocation_length-std_sizeof(OSD_CMD_LIST_DATA_HDR_t))
	  / sizeof(uint64_t) ) ;
}

static inline void osd_encap_cmd_list_data_add_entry(
    /*in*/ uint8_t *data_in_buf, uint64_t entry_val, 
    /*in*/uint64_t entry_num)
{
    uint64_t *pentry,ofs;
    osd_assert(data_in_buf);
    ofs = sizeof(uint64_t)*entry_num + std_sizeof(OSD_CMD_LIST_DATA_HDR_t);
    pentry= (uint64_t*)(data_in_buf+ofs);
    *pentry=HTONLL(entry_val);
}

static inline osd_bool_t osd_decap_cmd_list_data_get_entry(
    /*in*/ uint8_t *data_in_buf, uint64_t entry_idx/*zero based*/,
    /*out*/uint64_t *entry_val, uint64_t *tot_num)
{
    OSD_CMD_LIST_DATA_HDR_t *p_hdr =
	(OSD_CMD_LIST_DATA_HDR_t*)data_in_buf;
    uint64_t *pentry;
    uint64_t ofs;
    osd_assert(data_in_buf);
    osd_assert(entry_val);
    osd_assert(tot_num);
    ofs = sizeof(uint64_t)*entry_idx + std_sizeof(OSD_CMD_LIST_DATA_HDR_t);
    *tot_num = 
	NTOHLL(p_hdr->additional_length)/sizeof(uint64_t);

    if ( *tot_num > entry_idx ) {
	pentry= (uint64_t*)(data_in_buf+ofs);
	*entry_val=NTOHLL(*pentry);
	return OSD_TRUE;
    } else {
	return OSD_FALSE;
    }
}

/****************************************************** SET_KEY */

static inline void osd_encap_cdb_cmd_set_key(
    uint8_t *cdb,
    uint8_t key_to_set,
    uint64_desc_t src_partition_id, 
    uint8_desc_t src_key_ver, 
    const uint8_desc_t src_key_id[7], 
    const uint8_desc_t src_seed[20] )
{
    OSD_CDB_CMD_SET_KEY_t *p_cdb_cmd_set_key =
                    (OSD_CDB_CMD_SET_KEY_t*)cdb;
    OSD_CDB_HDR_t *phdr=(OSD_CDB_HDR_t *)cdb;
    osd_assert(cdb);
    osd_assert((key_to_set&0x3)==key_to_set);
    osd_assert(16>src_key_ver);
 
    SET_MASKED_VAL(phdr->options_byte2,
		   0x3,key_to_set);
    GENERIC_ENCAP_CDB_CMD_SET_KEY(
        p_cdb_cmd_set_key,
        src_partition_id,
        src_key_ver,
        src_key_id,
        src_seed );
    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
        PRINT("CDB_CMD_SET_KEY PARAMS:\n");
        GENERIC_PRINT_CDB_CMD_SET_KEY(p_cdb_cmd_set_key);
    }
}

static inline osd_bool_t osd_decap_cdb_cmd_set_key(
    const uint8_t *cdb, osd_result_t *result_p,
    uint8_t *dst_key_to_set,
    uint64_desc_t *dst_partition_id, 
    uint8_desc_t *dst_key_ver, 
    uint8_desc_t dst_key_id[7], 
    uint8_desc_t dst_seed[20] )
{
    const OSD_CDB_CMD_SET_KEY_t *p_cdb_cmd_set_key =
                    (const OSD_CDB_CMD_SET_KEY_t*)cdb;
    OSD_CDB_HDR_t *phdr=(OSD_CDB_HDR_t *)cdb;
    osd_assert(cdb);
    GENERIC_DECAP_CDB_CMD_SET_KEY(
        p_cdb_cmd_set_key,
        dst_partition_id,
        dst_key_ver,
        dst_key_id,
        dst_seed );
    *dst_key_to_set = (phdr->options_byte2 & 0x3);

    if (0xf0 & (*dst_key_ver)) {
	OSD_TRACE(OSD_TRACE_T10,1,"key_ver=%d is illegal. it must be within 0..15\n",(*dst_key_ver));
	result_p->rc=OSD_RC_INVALID_CDB;
	result_p->cdb_field_sense.bad_field_offset = 
	    &(p_cdb_cmd_set_key->key_ver) - cdb;
	result_p->cdb_field_sense.bad_field = OSD_UNKNOWN_FIELD;
	return OSD_FALSE;
    }
    
    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
        PRINT("CDB_CMD_SET_KEY PARAMS:\n");
        GENERIC_PRINT_CDB_CMD_SET_KEY(p_cdb_cmd_set_key);
    }
    return OSD_TRUE;
}

/****************************************************** DEBUG COMMAND */

static inline void osd_encap_cdb_cmd_debug(
    uint8_t *cdb,
    uint32_t data_out_length,
    uint32_t data_in_length)
{
    uint32_t *l_p;
    osd_assert(cdb);  

    l_p=(uint32_t*)&(cdb[16]); 
    *l_p=HTONL(data_out_length);
    l_p=(uint32_t*)&(cdb[20]);
    *l_p=HTONL(data_in_length);

    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
        PRINT("CDB_CMD_DEBUG PARAMS:\n");
	GEN_HDR_PRINT;
	UINT32_PRINT(16, "data_out_length", *(uint32_t*)(&(cdb[16])));
	UINT32_PRINT(20, "data_in_length", *(uint32_t*)(&(cdb[20])));
    }
}

static inline osd_bool_t osd_decap_cdb_cmd_debug(
    const uint8_t *cdb, osd_result_t *result_p,
    uint32_t *dst_data_out_length,
    uint32_t *dst_data_in_length)
{
    uint32_t *l_p;
    osd_assert(cdb);

    l_p=(uint32_t*)&(cdb[16]); 
    *dst_data_out_length = NTOHL(*l_p);
    l_p=(uint32_t*)&(cdb[20]);
    *dst_data_in_length = NTOHL(*l_p);

    if (osd_trace_is_set(OSD_TRACE_T10,2)) {
        PRINT("CDB_CMD_DEBUG PARAMS:\n");
	GEN_HDR_PRINT;
	UINT32_PRINT(16, "data_out_length", *(uint32_t*)(&(cdb[16])));
	UINT32_PRINT(20, "data_in_length", *(uint32_t*)(&(cdb[20])));
    }

    return OSD_TRUE;
}


#endif /*OSD_CDB_CMD_H*/

