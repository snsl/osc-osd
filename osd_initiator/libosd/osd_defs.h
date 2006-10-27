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

/**************
 * osd.h
 * definitions used by all osd components
 * Author: Allon Shafrir
 *******************************************************************/
#ifndef _OSD_DEFS_H
#define _OSD_DEFS_H

/******************************************************************
 * includes
 */

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

/******************************************************************
 * #defines
 */

#define SCSI_GOOD 0
#define SCSI_CHECK_CONDITION 2

#define OSD_CAPABILITY_SIZE 80

//size of cryptographic values used (keys, key-seeds, integrity check values)
#define OSD_CRYPTO_VAL_SIZE 20

#define OSD_CREDENTIAL_SIZE 120

//modifier for packed structs in gcc. used in define because w/o it confuses tags
#define OSD_PACKED __attribute__((packed))
#define OSD_PACKED_STRUCT_END //nothing is needed

//service actions - according to table1 48 in section 6 of OSD standard rev 10
#define OSD_CREATE            0x8802
#define OSD_CREATE_PARTITION  0x880B
#define OSD_GET_ATTR          0x880E
#define OSD_LIST              0x8803
#define OSD_REMOVE            0x880A
#define OSD_REMOVE_PARTITION  0x880C
#define OSD_READ              0x8805
#define OSD_SET_ATTR          0x880F
#define OSD_WRITE             0x8806
#define OSD_FORMAT            0x8801
#define OSD_SET_KEY           0x8818
#define OSD_INQUIRY           0x1200

//non standard service action for debugging - for internal use only
#define OSD_DEBUG             0x8F80 

//non standard service action for CLEAR command as agreed
#define OSD_CLEAR             0x8FA0

//unsupported commands
#define OSD_FLUSH             0x8808
#define OSD_FLUSH_PARTITION   0x881B
#define OSD_FLUSH_OSD         0x881C
#define OSD_APPEND            0x8807
#define OSD_CREATE_AND_WRITE  0x8812
#define OSD_CREATE_COLLECTION 0x8815
#define OSD_REMOVE_COLLECTION 0x8816
#define OSD_FLUSH_COLLECTION  0x881A
#define OSD_LIST_COLLECTION   0x8817
#define OSD_PERFORM_SCSI_CMD  0x8F7E
#define OSD_PERFORM_TSK_MNGN  0x8F7F
#define OSD_SET_MASTER_KEY    0x8819

//"virtual" service action codes only used internally by initiator
#define OSD_INQUIRY_83        0x1283
#define OSD_INQUIRY_B0        0x12B0
#define OSD_INQUIRY_B1        0x12B1

#define OSD_MAX_DEBUG_SENSE_LEN 255

#define OSD_SENSE_MAX_LEN (256)

#define OSD_CHANNEL_TOKEN_MAX_LEN 100 //max supported ch token

#define OSD_EUI64_COMPANY_ID 0x123456 //tbd!!! this is not a qualified company id


/******************************************************************
 * MACROS
 */
#define OSD_RC_IS_GOOD(rc) (OSD_RC_GOOD==rc)
#define OSD_SET_COMMAND(cmd, cmdlo, cmdhi)	do { cmd[1]=((cmd & 0xF0)>>4); cmd[0]=(cmd & 0x0F); } while(0)


/******************************************************************
 * type definitions
 */


//type used for 6-bytes time value on network
typedef struct {
    uint32_t minutes;
    uint16_t millisecs;
} OSD_PACKED uint48_t;
#define SIZEOF_uint48_t 6

//type used for encoded offset value on network
typedef uint32_t offset32_t;

//type used for 12 bytes values on network
typedef struct {
    uint8_t b[12];
} OSD_PACKED array12_t;
#define SIZEOF_array12_t 12

//types used for representation of fields in host (host order, not encoded)
#define uint8_desc_t uint8_t
#define uint16_desc_t uint16_t
#define uint32_desc_t uint32_t
#define uint64_desc_t uint64_t
#define uint48_desc_t uint64_t
#define offset32_desc_t uint64_t
#define array12_desc_t array12_t

//value used for unused offset field
#define OFFSET_IGNORE 0xFFFFFFFF

/* osd_rc_t is used as "flat-space-error-code
   reflecting the codes transferred in status&sense-data sent by a target */
typedef enum {
    OSD_RC_GOOD=0,
    OSD_RC_INVALID_OPCODE, //invalid opcode in cdb
    OSD_RC_INVALID_CDB,    //invalid field in cdb
    OSD_RC_INVALID_DATA_OUT,  //invalid field in params from data-out
    OSD_RC_NONEMPTY_PARTITION, //trying to delete nonempty partition
    OSD_RC_FROZEN_AUDIT,
    OSD_RC_FROZEN_KEY,
    OSD_RC_QUOTA_ERROR,
    OSD_RC_READ_PAST_END_OF_OBJECT, //read past end of user object
    OSD_RC_MEDIA_ERROR,
    OSD_RC_UNKNOWN_ERROR,

    OSD_RC_BAD= 0xff //until we get rid of it in all code
} osd_rc_t, Osd_rc;

typedef enum {
    OSD_UNKNOWN_FIELD=0, //default

    OSD_BAD_SERVICE_ACTION,

    OSD_BAD_PARTITION_ID,
    OSD_BAD_OBJECT_ID,
    OSD_BAD_LENGTH, //may be used if page-granularity is required
    OSD_BAD_STARTING_BYTE,

    OSD_BAD_LIST_ID,

    OSD_BAD_CREDENTIAL_EXIRATION_TIME,
    OSD_BAD_POL_ACCESS_TAG,
    OSD_BAD_CREATION_TIME,
    OSD_BAD_CAP_KEY_VER,
    OSD_BAD_CAPABILITY, //for other field in capability, we report the capability first bygte

    OSD_BAD_INTEGRITY_CHECK_VALUE,

    OSD_BAD_CDB_KEY_VER, //for set key

    OSD_BAD_CDB_ATTR_PARAMS, //marks first byte of attribute access parameters in cdb
} Osd_bad_cdb_field, osd_bad_cdb_field_t;

#define OSD_BAD_ATTR_GET_FIELD OSD_BAD_CDB_ATTR_PARAMS
#define OSD_BAD_ATTR_SET_FIELD OSD_BAD_CDB_ATTR_PARAMS


#define OSD_FUNC_VALIDATE    0x80000000
#define OSD_FUNC_CAP_V       0x20000000
#define OSD_FUNC_COMMAND     0x10000000
#define OSD_FUNC_IMP_ST_ATT  0x01000000
#define OSD_FUNC_SA_CAP_V    0x00200000
#define OSD_FUNC_SET_ATT     0x00100000
#define OSD_FUNC_GA_CAP_V    0x00020000
#define OSD_FUNC_GET_ATT     0x00010000

/* FLUSH OSD Constants */
#define FLUSH_OSD_PARTLIST 	0x00
#define FLUSH_OSD_ROOTATTRS 	0x01
#define FLUSH_OSD_EVERYTHING 	0x02

/*struct describing information passed in status&sense data*/
typedef struct {

    osd_rc_t rc;

    struct {
	uint16_t service_action;
	uint64_t partition_id;
	uint64_t object_id;

        //command functions state masks - use OSD_FUNC_... definitions
	uint32_t  not_initiated_cmd_funcs; 
	uint32_t  completed_cmd_funcs;
    } command_sense;

    int debug_sense_len;
    char debug_sense[OSD_MAX_DEBUG_SENSE_LEN];

    union {
	struct {
	    uint32_t page_num;
	    uint32_t attr_num;
	} quota_sense;

	struct {
	    uint16_t bad_field_offset;
	    osd_bad_cdb_field_t bad_field;
	} cdb_field_sense;

	struct {
	    uint16_t bad_field_offset;
	} data_field_sense;
	    
	struct {
	    uint64_t actually_read_bytes;
	} read_past_end_sense;
    };
 } osd_result_t, Osd_result;

typedef enum {
    OSD_FALSE=0,
    OSD_TRUE=1
} osd_bool_t, Osd_bool;

typedef struct {
    char channel_token_buf[OSD_CHANNEL_TOKEN_MAX_LEN];
    int  channel_token_length;
} osd_channel_token_t, Osd_channel_token;

/******************************************************************
 * functions
 */

/**
 *  Function:  osd_rc_name
 *
 *  Purpose:   get string holding osd_rc_t code name
 *
 *  Arguments: rc  - error code
 *
 *  Returns:   string holding rc code name
 *             or "unknown command" for unrecognized code.
 *
 **/
static inline const char * osd_rc_name(osd_rc_t rc);

/**
 *  Function:  service_name
 *
 *  Purpose:   get string holding service_action name
 *
 *  Arguments: service_action code
 *
 *  Returns:   string holding service name
 *             or "unknown command" for unrecognized code.
 *
 **/
static inline const char * service_name(uint16_t service_action);

//calculate target default EUI-64 device id from the target name
static inline uint64_t osd_calc_default_eui64_dev_id(const char * device_name_p);

//calculate LUN EUI-64 unique identifier
static inline uint64_t osd_calc_eui64_lun_id(uint64_t eui64_device_id, int lun);


/******************************************************************/


#define namecase(symbol) case symbol: return #symbol

/**
 *  Function:  osd_rc_name
 *
 *  Purpose:   get string holding osd_rc_t code name
 *
 *  Arguments: rc  - error code
 *
 *  Returns:   string holding rc code name
 *             or "unknown command" for unrecognized code.
 *
 **/
static inline const char * osd_rc_name(osd_rc_t rc) {
    switch (rc) {
	namecase(OSD_RC_GOOD);
	namecase(OSD_RC_INVALID_OPCODE); //invalid opcode in cdb
	namecase(OSD_RC_INVALID_CDB);    //invalid field in cdb
	namecase(OSD_RC_INVALID_DATA_OUT);  //invalid field in params from data-out
	namecase(OSD_RC_NONEMPTY_PARTITION); //trying to delete nonempty partition
	namecase(OSD_RC_FROZEN_AUDIT);
	namecase(OSD_RC_FROZEN_KEY);
	namecase(OSD_RC_QUOTA_ERROR);
	namecase(OSD_RC_READ_PAST_END_OF_OBJECT); //read past end of user object
	namecase(OSD_RC_MEDIA_ERROR);
	namecase(OSD_RC_UNKNOWN_ERROR);
	default: return "unknown RC_t value !!!";
    }
}

/**
 *  Function:  service_name
 *
 *  Purpose:   get string holding service_action name
 *
 *  Arguments: service_action code
 *
 *  Returns:   string holding service name
 *             or "unknown command" for unrecognized code.
 *
 **/
static inline const char * service_name(uint16_t service_action) {
    switch (service_action) {
	namecase( OSD_CREATE_PARTITION);
	namecase( OSD_REMOVE_PARTITION);
	namecase( OSD_CREATE);
	namecase( OSD_FORMAT);
	namecase( OSD_FLUSH);
	namecase( OSD_REMOVE);
	namecase( OSD_READ);
	namecase( OSD_WRITE);
	namecase( OSD_GET_ATTR);
	namecase( OSD_SET_ATTR);
	namecase( OSD_LIST);
	namecase( OSD_SET_KEY);
        namecase( OSD_CLEAR);
    default: return "unknown command !!!";
    }
}

static inline const char * osd_bad_field_name(osd_bad_cdb_field_t field) {
    switch (field) {
    namecase(OSD_BAD_SERVICE_ACTION);

    namecase(OSD_BAD_PARTITION_ID);
    namecase(OSD_BAD_OBJECT_ID);
    namecase(OSD_BAD_LENGTH);
    namecase(OSD_BAD_STARTING_BYTE);

    namecase(OSD_BAD_LIST_ID);

    namecase(OSD_BAD_CREDENTIAL_EXIRATION_TIME);
    namecase(OSD_BAD_POL_ACCESS_TAG);
    namecase(OSD_BAD_CREATION_TIME);
    namecase(OSD_BAD_CAP_KEY_VER);
    namecase(OSD_BAD_CAPABILITY);

    namecase(OSD_BAD_INTEGRITY_CHECK_VALUE);

    namecase(OSD_BAD_CDB_KEY_VER);

    namecase(OSD_BAD_CDB_ATTR_PARAMS);

    namecase(OSD_UNKNOWN_FIELD);

    default: return "unknown field code !!!";
    }
}

#undef namecase

static inline void osd_hash64_inject_num(uint64_t *seed, int input)
{
    uint64_t val,top,exp,bit;
    uint64_t max=0xFFFF; //generate dummy 16 bits random to roll seed a bit
    val=0;
    exp=1;
    top=1;
    
    //copy of osd_rand64 code with addition of c to seed

    if (*seed<(1<<30)) {
	*seed=(*seed)*69069L+1;
	*seed=(*seed)*69069L+1;
	*seed=(*seed)*69069L+1;
    }

    *seed += input;

    while ( (top<=max) && (exp/*exp didn't explode*/) ) {
	*seed=(*seed)*69069L+1;
	bit=(int)((*seed)&(1<<30))?1:0;
	val+=exp*bit;
	exp<<=1;
	top=val+exp;
    }
}

//calculate target default EUI-64 device id from the target name
static inline uint64_t osd_calc_default_eui64_dev_id(const char * device_name_p)
{
    uint64_t dev_id = 0;
    uint64_t company_id=0;
    uint64_t name_hash=0;
    uint64_t lun_base=0;
    const char * char_p;

    company_id = (0xFFFFFF & OSD_EUI64_COMPANY_ID);

    for (char_p=device_name_p; (char_p && (*char_p)) ; char_p++) {
	osd_hash64_inject_num(&name_hash, *char_p);
    }
    name_hash &= 0xFFFFFFFF ;

    lun_base = 0;
    
    dev_id = 
	(company_id << 40)
	| (name_hash << 8)
	| (lun_base);
	
    return dev_id;
}

//calculate LUN EUI-64 unique identifier
static inline uint64_t osd_calc_eui64_lun_id(uint64_t eui64_device_id, int lun)
{
    uint64_t lun_id = eui64_device_id;

    //integrate delta so that:
    //1. lun id of lun0 is dame as device id
    //2. assuming device id is unique, there wouldn't be (with high probability)
    //   collisions of lun-id's

    if (0==lun) 
	return  eui64_device_id;
    else {
	//calculate unique delta - assuming basic device id is unique
	osd_hash64_inject_num(&lun_id, lun);
	return lun_id;
    }
}

#endif /*_OSD_DEFS_H*/


/******************
 *  Change Log.
 *
 $Log: osd.h,v $
 Revision 1.18  2005/07/22 23:22:14  orodeh
 Initial work on OSD/GPFS. Getting the code to compile under a C++ compiler.

 Revision 1.17  2005/07/20 08:02:25  eitany
 extra # removed

 Revision 1.16  2005/05/05 18:03:07  shafrir
 added support for variable size channel token used in CAPKEY

 Revision 1.15  2005/04/13 16:01:39  shafrir
 changed api of security library for handling key management.
 bug fixes in key handling

 Revision 1.14  2005/03/14 13:40:33  shafrir
 updated attribute pages definitions according to revision 10 of the standard

 Revision 1.13  2005/02/28 14:29:07  eitany
 OSD_CLEAR defined twice

 Revision 1.12  2005/02/28 08:49:03  shafrir
 big fix

 Revision 1.11  2005/02/10 07:29:54  petra
 moved some definitions from sec.h

 Revision 1.10  2005/02/07 16:29:28  shafrir
 moved to revision 10 of the OSD standard
 combined .src file used for attr defs for oc and sim

 Revision 1.9  2005/02/06 07:16:52  dalit
 Modifications needed to generate an Open Source version of the Initiator:
 1. Added headers to the appropriate files
 2. Added a new makefile called Makefile_seagate under osd-initiator/kernel
 3. Added a make seagate_os_pkg option in the main Makefile which generates the OS package
 4. Added a sec_simple_admin.c to osd-initiator/kernel (a degenerate security library)
 5. Removed the seagate_test.c which is no longer in use

 Revision 1.8  2004/12/08 14:37:30  shafrir
 re-arranged bad field enum

 Revision 1.7  2004/12/08 10:15:57  shafrir
 adjusted trace calls

 Revision 1.6  2004/11/21 10:22:47  petra
 added definition to OSD_INQUIRY_83

 Revision 1.5  2004/11/18 18:34:58  shafrir
 added osd_bad_field_name() to convert bad field enum to string

 Revision 1.4  2004/11/17 15:45:03  shafrir
 added error code OSD_RC_UNKNOWN_ERROR and it's usage when sense
 decap'ing fails

 Revision 1.3  2004/11/17 11:16:49  shafrir
 added common functions to calculate eui-64 id's

 Revision 1.2  2004/11/17 08:56:12  orodeh
 Increasing sense size to an integer

 Revision 1.1  2004/11/10 16:27:00  shafrir
 t10/osd* moved to t10/osd/ except for osd_sec* files

 Revision 1.4  2004/11/09 21:24:52  shafrir
 Added Sense Handling code in osd_sense.h
 changed many return codes from osd_rc_t to boolean and added error
 recording

 Revision 1.3  2004/10/26 21:10:28  shafrir
 new security library

 Revision 1.2  2004/10/20 13:10:59  shafrir
 moved stuff from osd_formats.h to osd.h to allow security library
 h-files to be independent of osd_formats.h
 + rearranged this file

*/

