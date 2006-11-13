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

#ifndef OSD_ATTR_H
#define OSD_ATTR_H

/************************************************
 * File:osd_attr.h
 * constants, structs and functions for handling attributes
 */

#include "osd_formats.h"
#include "osd_attr_defs.h"

/****************************************
 * HOWTO GET ATTRIBUTE: (by initiator)
 * 1. build attr-num-list to get in data-out buffer(A)
 * 2. fill attr params in pre-initialized CDB in list-format (C)
 * send command to target, receive data-in buffer
 * 3. retrieve values from attr-num-and-val-list in data-in buffer
 */

/****************************************
 * HOWTO SET ATTRIBUTE: (by initiator)
 * 1. build attr-val-list to set in data-out buffer(B)
 * 2. fill attr params in pre-initialized CDB in list-format (C)
 * send command to target
 */

/*****************************************
 * A. building attr-num list
 * 1. use osd_attr_list_init_encap() to init a new list with type=OSD_ATTR_GET_LIST
 * 2. use osd_attr_num_list_iterate_encap() to add an attribute to list
 */

/*****************************************
 * B. building attr-val-list list
 * 1. use osd_attr_list_init_encap() to init a new list with appropriate list type
 * 2. use osd_attr_val_list_iterate_encap() to add an attribute to list
 */

/*****************************************
 * C. setting atributes params in list-format in CDB
 * 1. cdb must have been initialized before with osd_encap_cdb_hdr()
 * 2. use osd_encap_cdb_attr_list(...) from osd_cdb.h to set params
 */

//list types used in api function parameters
typedef enum osd_attr_list_type_t {
    OSD_ATTR_INVALID_LIST_TYPE,
    OSD_ATTR_GET_LIST,
    OSD_ATTR_SET_LIST,
    OSD_ATTR_RET_LIST,
    OSD_ATTR_MULTIOBJ_RET_LIST,
} osd_attr_list_type_t, Osd_attr_list_type;

//types of standard attribute-lists in data-in/data-out buffer
#define OSD_ATTR_LIST_TYPE_MASK 0xF
#define OSD_ATTR_LIST_TYPE_NUM  0x1 //used to list attr to get
#define OSD_ATTR_LIST_TYPE_VAL  0x9 //used to retrieve attr val/set attr val
#define OSD_ATTR_LIST_TYPE_MULTIVAL 0x0F //used to retrieve attr val for 
                                //create cmd creating multiple objects

/*definitions for reading/writing attribute lists:*/

typedef struct {
    uint8_t byte0;
    uint8_t reserved1[1];
    uint16_t list_length;
    PACKED_STRUCT_END;
} PACKED OSD_ATTR_LIST_HDR_t;
#define SIZEOF_OSD_ATTR_LIST_HDR_t (1+1+2)

typedef struct {
    uint32_t page_num;
    uint32_t attr_num;
    PACKED_STRUCT_END;
} PACKED OSD_ATTR_LIST_NUM_ENTRY_t;
#define SIZEOF_OSD_ATTR_LIST_NUM_ENTRY_t (4+4)

typedef struct {
    uint32_t page_num;
    uint32_t attr_num;
    uint16_t attr_val_len;
    PACKED_STRUCT_END;
} PACKED OSD_ATTR_LIST_VAL_ENTRY_HDR_t;
#define SIZEOF_OSD_ATTR_LIST_VAL_ENTRY_HDR_t (4+4+2)

typedef struct {
    uint64_t obj_id;
    uint32_t page_num;
    uint32_t attr_num;
    uint16_t attr_val_len;
    PACKED_STRUCT_END;
} PACKED OSD_ATTR_LIST_MULTIVAL_ENTRY_HDR_t;
#define SIZEOF_OSD_ATTR_LIST_MULTIVAL_ENTRY_HDR_t (8+4+4+2)

typedef struct {
    Osd_attr_list_type list_type;
    osd_bool_t under_construction;
    OSD_ATTR_LIST_HDR_t *hdr_p;
    uint8_t *data;
    uint16_t max_list_data_len;
    uint16_t current_entry_index; /*zero based index*/
    uint16_t current_entry_ofs;
    uint16_t last_entry_ofs;
    uint16_t entry_hdr_len;//usefull constant according to list type
} attr_list_desc_t;

/*following functions should be used to read/write attribute list buffers */

static const char * osd_attr_list_type_name(Osd_attr_list_type list_type) 
{
    switch (list_type) {
    case OSD_ATTR_GET_LIST: return "OSD_ATTR_GET_LIST";
    case OSD_ATTR_SET_LIST: return "OSD_ATTR_SET_LIST";
    case OSD_ATTR_RET_LIST: return "OSD_ATTR_RET_LIST";
    case OSD_ATTR_MULTIOBJ_RET_LIST: return "OSD_ATTR_MULTIOBJ_RET_LIST";
    default: return "OSD_ATTR_INVALID_LIST_TYPE";
    }
}

/* osd_attr_list_entry_hdr_size
   purpose: returns size of header for each entry in list of type list_type
   returns: size of header for valid list_type. 0 on error (invalid type given)
*/
static inline uint16_t osd_attr_list_entry_hdr_size(Osd_attr_list_type list_type)
{
    switch (list_type) {
    case OSD_ATTR_GET_LIST: 
	return std_sizeof(OSD_ATTR_LIST_NUM_ENTRY_t);

    case OSD_ATTR_SET_LIST:
    case OSD_ATTR_RET_LIST:
	return std_sizeof(OSD_ATTR_LIST_VAL_ENTRY_HDR_t);
    case OSD_ATTR_MULTIOBJ_RET_LIST:
	return std_sizeof(OSD_ATTR_LIST_MULTIVAL_ENTRY_HDR_t);
    default:
	OSD_TRACE(OSD_TRACE_T10,1,
		  "invalid attributes-list type %d\n",(int)list_type);
	return 0;
    }
}

static inline osd_bool_t osd_attr_check_list_type(
    const OSD_ATTR_LIST_HDR_t *list_hdr_p,
    Osd_attr_list_type expected_list_type)
{
    uint8_t list_hdr_type;

    osd_assert(list_hdr_p);

    list_hdr_type = list_hdr_p->byte0&OSD_ATTR_LIST_TYPE_MASK;
    
    switch (expected_list_type) {
    case OSD_ATTR_GET_LIST: 
	return (OSD_ATTR_LIST_TYPE_NUM==list_hdr_type)?OSD_TRUE:OSD_FALSE;

    case OSD_ATTR_SET_LIST:
    case OSD_ATTR_RET_LIST:
	return (OSD_ATTR_LIST_TYPE_VAL==list_hdr_type)?OSD_TRUE:OSD_FALSE;

    case OSD_ATTR_MULTIOBJ_RET_LIST:
	return (OSD_ATTR_LIST_TYPE_MULTIVAL==list_hdr_type)?OSD_TRUE:OSD_FALSE;

    default:
	OSD_TRACE(OSD_TRACE_T10,1,
		  "invalid attributes-list type %d\n",(int)expected_list_type);
	return OSD_FALSE;
    }
}

static inline uint8_t osd_attr_encap_list_type(Osd_attr_list_type list_type)
{
    switch (list_type) {
    case OSD_ATTR_GET_LIST: 
	return OSD_ATTR_LIST_TYPE_NUM;

    case OSD_ATTR_SET_LIST:
    case OSD_ATTR_RET_LIST:
	return OSD_ATTR_LIST_TYPE_VAL;

    case OSD_ATTR_MULTIOBJ_RET_LIST:
	return OSD_ATTR_LIST_TYPE_MULTIVAL;

    default:
	OSD_TRACE(OSD_TRACE_T10,1,
		  "invalid attributes-list type %d\n",(int)list_type);
	return 0;
    }
}

static inline void osd_attr_list_update_offset(attr_list_desc_t *list_desc_p,
					       int new_offset)
{
    osd_assert(list_desc_p);
    osd_assert(list_desc_p->current_entry_ofs <= new_offset);
    list_desc_p->last_entry_ofs=list_desc_p->current_entry_ofs;
    list_desc_p->current_entry_ofs=new_offset;

    if (list_desc_p->under_construction) {
	switch (list_desc_p->list_type) {
	case OSD_ATTR_GET_LIST:
	case OSD_ATTR_SET_LIST:
	    osd_assert(0==list_desc_p->hdr_p->list_length);
	    break;
	case OSD_ATTR_RET_LIST:
	case OSD_ATTR_MULTIOBJ_RET_LIST:
	    list_desc_p->hdr_p->list_length = HTONS(new_offset);
	    break;
	default:
	    osd_assert(0);
	}
    }
}

/*osd_attr_list_init_decap()
 *  in: dout buffer pointer and length to use
 *  out: preallocated list_desc_p to be filled by function
 *       initialized list_desc_p will be later used for iterating functions
 * RETURN VALUE: OSD_RC_GOOD upon success, other value on failure.
 * possible failure reasons: 
 *   unrecognized format of buffer
 *   too-small list size.
 * 
 */
static inline osd_rc_t osd_attr_list_init_decap(
    /*IN:*/ 
    const uint8_t *list_buf_p, 
    int list_buf_len, 
    osd_attr_list_type_t expected_list_type,
    /*OUT:*/
    attr_list_desc_t *list_desc_p) 
{
    osd_assert(list_desc_p);
    osd_assert(list_buf_p);
    osd_assert(list_buf_len<=UINT16_MAX);

    list_desc_p->hdr_p=(OSD_ATTR_LIST_HDR_t *)list_buf_p;
    list_desc_p->data=(uint8_t *)list_buf_p+std_sizeof(OSD_ATTR_LIST_HDR_t);
    list_desc_p->current_entry_index=0;
    list_desc_p->current_entry_ofs=0;
    list_desc_p->under_construction=OSD_FALSE;

    if ( ! osd_attr_check_list_type(list_desc_p->hdr_p,
				    expected_list_type) ) {
	osd_trace(OSD_TRACE_T10,1,
		  "unexpected list type within list data for a %s\n",
		  osd_attr_list_type_name(expected_list_type) );
	return OSD_RC_BAD;
    }
    list_desc_p->list_type = expected_list_type;

    
    switch (list_desc_p->list_type) {
    case OSD_ATTR_GET_LIST:
    case OSD_ATTR_SET_LIST:
	//clients always fill zero in list_length field in the header
	//so we only use the buf size gotten from the CDB
	list_desc_p->max_list_data_len = 
	    list_buf_len - std_sizeof(OSD_ATTR_LIST_HDR_t);
	break;
    default:
	//use the list_length field from the list header
	list_desc_p->max_list_data_len = NTOHS(list_desc_p->hdr_p->list_length);
	break;
    }

    OSD_TRACE(OSD_TRACE_T10,2,"attr list type:0x%x, buf_len=%d "
	      "current_contents_len:%d\n",
	      (int)list_desc_p->list_type,
	      list_buf_len,
	      list_desc_p->max_list_data_len);

    list_desc_p->entry_hdr_len = osd_attr_list_entry_hdr_size(list_desc_p->list_type);
    if (0==list_desc_p->entry_hdr_len) {
	return OSD_RC_BAD;
    }

    if ( (list_desc_p->max_list_data_len)<list_desc_p->entry_hdr_len) {
	OSD_TRACE(OSD_TRACE_T10,1,
		  "list content length(=%d) smaller then single entry hdr\n",
		  list_desc_p->max_list_data_len);
	return OSD_RC_BAD;
    }
    
    if ( (list_desc_p->max_list_data_len+std_sizeof(OSD_ATTR_LIST_HDR_t)) > list_buf_len )
    {
	OSD_TRACE(OSD_TRACE_T10,1,
		  "list content length = %d+%d > list_buf_length=%d\n",
		  list_desc_p->max_list_data_len,std_sizeof(OSD_ATTR_LIST_HDR_t),
		  list_buf_len);
	return OSD_RC_BAD;
    }
    
//zero-iteration:
    
    osd_attr_list_update_offset(list_desc_p, 0);

    if ( (list_desc_p->current_entry_ofs)>(list_desc_p->max_list_data_len) ) {
	return OSD_RC_BAD; //no more entries possible
    }
    if ( (list_desc_p->max_list_data_len)<list_desc_p->entry_hdr_len) {
	OSD_TRACE(OSD_TRACE_T10,1,
		  "list content length(=%d) smaller then single entry hdr\n",
		  list_desc_p->max_list_data_len);
	return OSD_RC_BAD;
    }

    return OSD_RC_GOOD;
}

/*osd_attr_list_init_encap()
 *  IN: list buffer, length, and type of list being created
 *  OUT: preallocated list_desc_p to fill
 *       initialized list_desc_p will be later used for iterating functions
 * RETURN VALUE: OSD_RC_GOOD upon success, other value on failure.
 * possible failure reasons: 
 *   too-small list size.
 */
static inline RC_t osd_attr_list_init_encap(
    /*IN*/ 
    uint8_t *list_buf_p, int list_buf_len, 
    osd_attr_list_type_t list_type,
    /*OUT*/
    attr_list_desc_t *list_desc_p) 
{
    OSD_ATTR_LIST_HDR_t *hdr_p=(OSD_ATTR_LIST_HDR_t *)list_buf_p;

    osd_assert(list_desc_p);
    osd_assert(list_buf_p);
    osd_assert(list_buf_len<=UINT16_MAX);

    list_desc_p->hdr_p=(OSD_ATTR_LIST_HDR_t *)list_buf_p;
    list_desc_p->data=(uint8_t *)list_buf_p+std_sizeof(OSD_ATTR_LIST_HDR_t);
    list_desc_p->list_type=list_type;
    list_desc_p->current_entry_index=0;
    list_desc_p->current_entry_ofs=0;
    list_desc_p->max_list_data_len = list_buf_len-std_sizeof(OSD_ATTR_LIST_HDR_t);
    list_desc_p->under_construction=OSD_TRUE;

    OSD_TRACE(OSD_TRACE_T10,2,"attr list type:0x%x, buf_len=%d "
	  "current_contents_len:%d\n",
	  list_type,list_buf_len,list_desc_p->current_entry_ofs);

    list_desc_p->entry_hdr_len = osd_attr_list_entry_hdr_size(list_desc_p->list_type);
    if (0==list_desc_p->entry_hdr_len) {
	return OSD_RC_BAD;
    }

    memset(list_buf_p,0,list_buf_len);
    hdr_p->byte0=osd_attr_encap_list_type(list_type);

//zero-iteration:
    
    osd_attr_list_update_offset(list_desc_p, 0);
    if ( ( (list_desc_p->current_entry_ofs) +
	   list_desc_p->entry_hdr_len        ) > (list_desc_p->max_list_data_len) ) {
	OSD_TRACE(OSD_TRACE_T10,1,
		  "list len(%d) too small to fit one entry.\n",
		  list_buf_len);
	return OSD_RC_BAD; //no more entries can be added
    }
    return OSD_RC_GOOD;
}

/* osd_attr_get_list_length()
 * Purpose: returns current entries length(w/o header) of attributes-list
 * INT: @list_desc_p ptr to list descriptor previousely initialized by either
 *       osd_attr_list_init_decap or osd_attr_list_init_encap function.
 * RETURN VALUE: length of entries part in list.
 */
static inline uint16_t 
osd_attr_get_list_entries_length(const attr_list_desc_t *list_desc_p )
{
    osd_assert(list_desc_p);
    return list_desc_p->current_entry_ofs;
}

/* osd_attr_get_list_length()
 * Purpose: returns current total length(including header) of attributes-list
 * INT: @list_desc_p ptr to list descriptor previousely initialized by either
 *        osd_attr_list_init_decap or osd_attr_list_init_encap function.
 * RETURN VALUE: total length of buffer holding the list.
 */
static inline uint16_t 
osd_attr_get_list_buf_length(const attr_list_desc_t *list_desc_p )
{
    return ( std_sizeof(OSD_ATTR_LIST_HDR_t)+    
	     osd_attr_get_list_entries_length(list_desc_p) ) ;
}

/* osd_attr_list_last_entry_offset()
 * Purpose: returns offste within list of the beginning of the last entry
 * INT: @list_desc_p ptr to list descriptor previousely initialized by either
 *        osd_attr_list_init_decap or osd_attr_list_init_encap function.
 * RETURN VALUE: offset within the list buffer of the last entry
 */
static inline uint16_t 
osd_attr_list_last_entry_offset(const attr_list_desc_t *list_desc_p )
{
    return ( std_sizeof(OSD_ATTR_LIST_HDR_t)+    
	     list_desc_p->last_entry_ofs ) ;
}

/*************************************************************** num lists **/

/* osd_attr_num_list_iterate_decap()
 * can be called repeatedly until first time it returns val!=OSD_RC_GOOD
 * INOUT: initialized num-type list_desc_p. holds list iterations state and 
 *        buffer ptr to parse
 * OUT:   pre-allocated entry_desc to be filled.
 * RETURN VALUE: OSD_RC_GOOD upon success, other value on failure.
 * faulure reasons: no more entries in list
 */
static inline RC_t osd_attr_num_list_iterate_decap(
    /*INOUT*/ attr_list_desc_t *list_desc_p,
    /*OUT*/   uint32_t *page_num_p, uint32_t *attr_num_p) 
{
    const OSD_ATTR_LIST_NUM_ENTRY_t *entry_p;
    uint16_t ofs;
    
    osd_assert(page_num_p);
    osd_assert(attr_num_p);
    osd_assert(list_desc_p);
    osd_assert(list_desc_p->hdr_p);
    osd_assert(list_desc_p->data);
    osd_assert(OSD_ATTR_GET_LIST == list_desc_p->list_type);

    ofs = list_desc_p->current_entry_ofs;
    entry_p = (const OSD_ATTR_LIST_NUM_ENTRY_t*)((list_desc_p->data)+ofs);

    //if we're exactly at end of list, exit quetly
    if (ofs==(list_desc_p->max_list_data_len)) {
      return OSD_RC_BAD; //no more entries in list
    }
    
    //calc new offset in list data
    ofs+= list_desc_p->entry_hdr_len;
    if (ofs>(list_desc_p->max_list_data_len)) {
      OSD_TRACE(OSD_TRACE_T10,1,
		"error. at end of entry hdr. ofs=%d > list_contents_len=%d\n",
		ofs, list_desc_p->max_list_data_len);
	return OSD_RC_BAD; //no more entries in list
    }
    *page_num_p = NTOHL(entry_p->page_num);
    *attr_num_p = NTOHL(entry_p->attr_num);

    list_desc_p->current_entry_index++;
    osd_attr_list_update_offset(list_desc_p, ofs);

    OSD_TRACE(OSD_TRACE_T10,2,"decaped (page:0x%x,attr:0x%x)\n",
	  *page_num_p,*attr_num_p);
    return OSD_RC_GOOD;
}

/* osd_attr_num_list_iterate_encap()
 * can be called repeatedly until first time it returns value!=OSD_RC_GOOD
 * meaning no more entries can be added to list
 * INOUT: initialized num-type list_desc_p. 
 *          holds list iterations state and written buf ptr
 * IN:    pre-allocated entry_desc to be added to list
 * RETURN VALUE: OSD_RC_GOOD upon success, other value on failure.
 * faulure reasons: no room for entry in list
 */
static inline RC_t osd_attr_num_list_iterate_encap(
    /*INOUT*/ attr_list_desc_t *list_desc_p,
    /*IN*/    uint32_t page_num, uint32_t attr_num) 
{
    OSD_ATTR_LIST_NUM_ENTRY_t *entry_p;
    uint16_t ofs;
    
    osd_assert(list_desc_p);
    osd_assert(list_desc_p->hdr_p);
    osd_assert(list_desc_p->data);
    osd_assert(OSD_ATTR_GET_LIST == list_desc_p->list_type);

    ofs = list_desc_p->current_entry_ofs;
    entry_p = (OSD_ATTR_LIST_NUM_ENTRY_t*)((list_desc_p->data)+ofs);

    //calc new offset in list data
    ofs+= list_desc_p->entry_hdr_len;
    if (ofs>(list_desc_p->max_list_data_len)) {
      OSD_TRACE(OSD_TRACE_T10,1,
		"error. at end of entry hdr. ofs=%d > max_contents_len=%d\n",
		ofs, list_desc_p->max_list_data_len);
	return OSD_RC_BAD; //no more entries can be added
    }
    entry_p->page_num = HTONL(page_num);
    entry_p->attr_num = HTONL(attr_num);

    list_desc_p->current_entry_index++;
    osd_attr_list_update_offset(list_desc_p, ofs);

    OSD_TRACE(OSD_TRACE_T10,2,"encaped (page:0x%x,attr:0x%x)\n",
	  page_num,attr_num);
    return OSD_RC_GOOD;
}

/*************************************************************** val lists **/

/* osd_attr_val_list_iterate_decap()
 * can be called repeatedly until first time it returns value!=OSD_RC_GOOD
 * INOUT: initialized val-type list_desc_p. holds list iterations state and 
 *        buffer ptr to parse
 * OUT:   pre-allocated entry_desc to be filled.
 *        attr_val_ptr inside entry_desc is set by this function
 *        and points to a location in parsed list buffer 
 *        (should be read from there before buffer is freed)
 * RETURN VALUE: OSD_RC_GOOD upon success, other value on failure.
 * faulure reasons: no more entries in list
 */
static inline RC_t osd_attr_val_list_iterate_decap(
    /*INOUT*/ attr_list_desc_t *list_desc_p,
    /*OUT*/   uint32_t *page_num_p, uint32_t *attr_num_p,
    /*OUT*/   uint16_t *full_attr_len_p, 
    /*OUT*/   uint16_t *decaped_val_len_p, uint8_t **attr_val_buf_pp )
{
    const OSD_ATTR_LIST_VAL_ENTRY_HDR_t *entry_p;
    uint8_t *pdata;
    uint16_t ofs,max_len;
    
    osd_assert(page_num_p);
    osd_assert(attr_num_p);
    osd_assert(full_attr_len_p);
    osd_assert(decaped_val_len_p);
    osd_assert(attr_val_buf_pp);
    osd_assert(list_desc_p);
    osd_assert(list_desc_p->hdr_p);
    osd_assert(list_desc_p->data);
    osd_assert(  (OSD_ATTR_RET_LIST == list_desc_p->list_type) ||
		 (OSD_ATTR_SET_LIST == list_desc_p->list_type) );

    *attr_val_buf_pp=NULL;//assume fail
    ofs = list_desc_p->current_entry_ofs;

    //if we're exactly at end of list, exit quetly
    if (ofs==(list_desc_p->max_list_data_len)) {
      return OSD_RC_BAD; //no more entries in list
    }

    entry_p = (const OSD_ATTR_LIST_VAL_ENTRY_HDR_t *)((list_desc_p->data)+ofs);
    //calc new offset in list data
    ofs+= list_desc_p->entry_hdr_len;
    if (ofs>(list_desc_p->max_list_data_len)) {
      OSD_TRACE(OSD_TRACE_T10,1,
		"error. at end of entry hdr. ofs=%d > list_contents_len=%d\n",
		ofs, list_desc_p->max_list_data_len);
	return OSD_RC_BAD; //no more entries can be added
    }
    //make sure all is well
    pdata=(list_desc_p->data)+ofs;
    *page_num_p = NTOHL(entry_p->page_num);
    *attr_num_p = NTOHL(entry_p->attr_num);
    *full_attr_len_p = NTOHS(entry_p->attr_val_len);
    max_len=list_desc_p->max_list_data_len-ofs;

    if (OSD_ATTR_UNDEFINED_VAL_LENGTH == *full_attr_len_p) {
	*decaped_val_len_p=0;
    } else if (*full_attr_len_p>max_len) {
	*decaped_val_len_p=max_len;
    } else {
	*decaped_val_len_p=*full_attr_len_p;
    }
    
    ofs+= *decaped_val_len_p;
    osd_assert(ofs<=list_desc_p->max_list_data_len);
    
    *attr_val_buf_pp=pdata;

    list_desc_p->current_entry_index++;
    osd_attr_list_update_offset(list_desc_p, ofs);
    
    OSD_TRACE(OSD_TRACE_T10,2,"decaped val (page:0x%x,attr:0x%x) len=%d of %d\n",
	      *page_num_p,*attr_num_p,
	      *decaped_val_len_p,*full_attr_len_p);
    return OSD_RC_GOOD;
}

/* osd_attr_val_list_prefetch_val_ptr
 * returns pointer for next value to be encaped
 * IN: pre-initialized list_desc_p holdinf iteration status and written buf
 * OUT: *val_buf points to location in for next-value in written buffer
 *      *max_len - max bytes to write to val_buf address.
 *
 * *val_buf should be immediately filled with up do max_len bytes and then 
 * an entry_desc with attr_val_buf=*val_buf should be prepared
 * osd_attr_val_list_iterate_encap should be called for it.
 * RETURN VALUE: OSD_RC_GOOD upon success, other value on failure.
 * faulure reasons: no room for entry in list
 */
static inline RC_t osd_attr_list_prefetch_val_ptr(
    /*IN*/const attr_list_desc_t *list_desc_p,
    /*OUT*/uint16_t *max_len_p, uint8_t **val_buf_pp)
{
    uint16_t ofs;
    osd_assert(list_desc_p);
    osd_assert(list_desc_p->hdr_p);
    osd_assert(list_desc_p->data);
    osd_assert( (OSD_ATTR_SET_LIST==list_desc_p->list_type) ||
		(OSD_ATTR_RET_LIST==list_desc_p->list_type) ||
		(OSD_ATTR_MULTIOBJ_RET_LIST==list_desc_p->list_type) );

    *val_buf_pp=NULL; *max_len_p=0; //assume fail
    
    ofs=list_desc_p->current_entry_ofs+list_desc_p->entry_hdr_len;
    if (ofs > list_desc_p->max_list_data_len) {
	OSD_TRACE(OSD_TRACE_T10,1,
		  "warning. no more room to encap attributes."
		  "cur_ofs+hdr=%d > max_contents_len=%d\n",
		  ofs, list_desc_p->max_list_data_len);
	return OSD_RC_BAD;
    }
    *max_len_p=(list_desc_p->max_list_data_len-ofs);
    *val_buf_pp=(list_desc_p->data)+ofs;
    return OSD_RC_GOOD;
}



/* osd_attr_val_list_iterate_encap()
 * can be called repeatedly until first time it returns value!=OSD_RC_GOOD
 * meaning no more entries can be added to list
 * INOUT: initialized num-type list_desc_p. 
 *          holds list iterations state and written buf ptr
 * IN:    pre-allocated entry_desc to be added to list
 *        attr_val_buf should be a preset pointer to the correct location in 
 *        the list-buffer (calculated by osd_attr_val_list_prefetch_val_ptr)
 *        attr_val_buf should have been filled with value already.
 * RETURN VALUE: OSD_RC_GOOD upon success, other value on failure.
 * faulure reasons: no room for entry in list
 */
static inline RC_t osd_attr_val_list_iterate_encap(
    /*INOUT*/ attr_list_desc_t *list_desc_p,
    /*IN*/    uint32_t page_num, uint32_t attr_num,
    /*IN*/    int full_attr_len,
    /*IN*/    int encap_val_len, uint8_t *attr_val_buf)
{
    OSD_ATTR_LIST_VAL_ENTRY_HDR_t *entry_p;
    uint8_t *pdata;
    uint16_t ofs;
    
    osd_assert(attr_val_buf);
    osd_assert(list_desc_p);
    osd_assert(list_desc_p->hdr_p);
    osd_assert(list_desc_p->data);
    osd_assert(full_attr_len<=UINT16_MAX);
    osd_assert(encap_val_len<=UINT16_MAX);
    osd_assert(  (OSD_ATTR_RET_LIST == list_desc_p->list_type) ||
		 (OSD_ATTR_SET_LIST == list_desc_p->list_type) );

    ofs = list_desc_p->current_entry_ofs;
    entry_p = (OSD_ATTR_LIST_VAL_ENTRY_HDR_t *)((list_desc_p->data)+ofs);

    //calc new offset in list data
    ofs+= list_desc_p->entry_hdr_len;
    pdata=(list_desc_p->data)+ofs;
    osd_assert(attr_val_buf==pdata);
    ofs+= encap_val_len;
    // check max_len returned by prefetch wasn't violated
    osd_assert(ofs<=(list_desc_p->max_list_data_len));

    // check truncation of value was done only if we're at end of list buffer
    if ( (encap_val_len!=full_attr_len) &&
	 (ofs<list_desc_p->max_list_data_len) &&
	 (full_attr_len!=OSD_ATTR_UNDEFINED_VAL_LENGTH) ) {
	OSD_TRACE(OSD_TRACE_T10,1,
		  "encaped value truncated while list-buffer not full.\n");
    }
    entry_p->page_num = HTONL(page_num);
    entry_p->attr_num = HTONL(attr_num);
    entry_p->attr_val_len = HTONS(full_attr_len);
    
    list_desc_p->current_entry_index++;
    osd_attr_list_update_offset(list_desc_p,ofs);

    OSD_TRACE(OSD_TRACE_T10,2,"encaped val (page:0x%x,attr:0x%x) len=%d of %d\n",
	  page_num,attr_num,encap_val_len,full_attr_len);
    return OSD_RC_GOOD;
}
#if 0
/****************************************************** other attr stuff */

typedef struct {
    char vendor_id[8];
    char page_id[32];
} osd_attr0_t;

static inline void osd_attr_encap_page_id_uint32(uint32_t *dst_p,
						 uint32_t page_num)
{ 
    osd_assert(dst_p);
    *dst_p=HTONL(page_num); 
}

static inline void osd_attr_decap_page_id_uint32(const uint32_t *src_p,
						 uint32_t *page_num_p)
{ 
    osd_assert(src_p);
    osd_assert(page_num_p);
    *page_num_p=NTOHL(*src_p); 
}

/* osd_attr_encap_page_id_string
 * build long format of attr 0 value for specified page_num
 */
static inline void osd_attr_encap_page_id_string(osd_attr0_t *dst,
					  const char* vendor_id,
					  const char* page_id) 
{
    osd_assert(dst);
    osd_assert(vendor_id);
    osd_assert(page_id);
    osd_str_pad_copy(dst->vendor_id,vendor_id,8,' ');
    osd_str_pad_copy(dst->page_id,page_id,32,' ');
}

static inline void osd_attr_decap_page_id_string(const osd_attr0_t *src,
					  char vendor_id[9],
					  char page_id[33]) 
{
    osd_assert(src);
    osd_assert(vendor_id);
    osd_assert(page_id);
    osd_str_unpad_copy(vendor_id,src->vendor_id,8,' ');
    osd_str_unpad_copy(page_id,src->page_id,32,' ');
}
#endif
#endif /*ifndef OSD_ATTR_H */
