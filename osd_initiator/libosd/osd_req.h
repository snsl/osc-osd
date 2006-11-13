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
/*
 *  Header defintitions for the SCSI OSD driver
 *
 *  Written by Liran Schour <lirans@il.ibm.com>
 *
 *  Imported API: (should be exported by using application)
 *  =============
 *   params controlling debug level:
 *   ===============================
 *                  g_osd_trave_include (set to zero to disable traces)
 *                  g_osd_trave_exclude (set to 0xffffffff to disable traces)
 *
 *  Exported API: ( see README for usage example )
 *  =============
 *   device handle API:
 *   ==================
 *                  get_osd_dev
 *                  get_osd_dev_by_id
 *                  put_osd_dev
 *                  get_osd_channel_token
 *                  get_osd_system_id
 *                
 *   Prepare main OSD command API: ( returns a handle to an OSD request )
 *   ========================================
 *                  prepare_osd_create_partition
 *                  prepare_osd_format_lun 
 *                  prepare_osd_create 
 *                  prepare_osd_write 
 *                  prepare_osd_clear
 *                  prepare_osd_read
 *                  prepare_osd_inquiry
 *                  prepare_osd_list 
 *                  prepare_osd_remove 
 *                  prepare_osd_remove_partition 
 *                  prepare_osd_set_attr
 *                  prepare_osd_get_attr
 *                  prepare_osd_set_key
 *                  prepare_osd_debug
 *
 *   set OSD attributes params API: (iterative)
 *   ==============================
 *                  prepare_get_attr_list_add_entry
 *                  prepare_set_attr_list_add_entry
 *
 *   Submission of OSD request:
 *   ==========================
 *                  do_osd_req 
 *                      OR
 *                  wait_osd_req 
 *
 *   OSD request status indication API:
 *   ==================================
 *                  osd_get_result 
 *                  osd_get_sense    
 *
 *   Retrieve attributes and info from request API:
 *   ==============================================
 *                  init_extract_attr_from_req 
 *                  extract_next_attr_from_req (iterative) 
 *                  extract_list_from_req (iterative)                 
 *                  extract_id_from_inquiry_data
 *
 *   Asynchronous mechanisms API:
 *   =============================
 *                  create_osd_context 
 *                  destroy_osd_ctx 
 *                  get_completed_osd_req 
 *
 *   Free an OSD request:
 *   ====================
 *                  free_osd_req
 * 
 *   Get status of OSD initiator:
 *   ============================
 *                  get_osd_load
 */               
#ifndef OSD_REQ_H
#define OSD_REQ_H

#ifndef __KERNEL__
#define EXPORT_SYMBOL(x)
#else
#include <linux/module.h>
#endif

#include "osd.h" /* include OSD protocol dependent definitions */

#define MAX_OSD_IOV_COUNT 32

#define OSD_DEFAULT_TIMEOUT 45  
#define OSD_DEFAULT_RETRIES 1

#ifdef __KERNEL__
typedef dev_t osd_dev_handle_t;
#else
typedef int osd_dev_handle_t;
#endif

/* variables controlling output trace level
   these should be implemented and exported by using application
   they are used by osd_req_module_init() for setting the debug level
*/
extern uint32_t g_osd_trace_exclude,g_osd_trace_include;


/**
 *
 *    Function: get_osd_dev
 *
 *    Description:  returnes a reference to the requested osd_dev# OSD device
 *                  attached in the system
 *    Arguments:    @osd_dev - ordinal number of the OSD device registered 
 *                  in the system.
 *                      legal value: osd_dev >= 0
 *
 *    Notes:        It is recommended to use device identifer to create a 
 *                  correct reference to an OSD device
 *
 *    Return codes: handle to the device
 *
 **/
extern osd_dev_handle_t get_osd_dev(int osd_dev);


/**
 *
 *    Function: get_osd_dev_by_id
 *
 *    Description:  returnes a reference to the requested osd_dev# OSD device
 *                  attached in the system
 *    Arguments:    @osd_dev_id - unique identifier of the OSD device
 *                  registered in the system
 *                  @dev_lun - lun number on the target device.
 *                      legal value: dev_lun >= 0
 *
 *    Notes:        It is recommended to use device identifer to create a 
 *                  correct reference to an OSD device
 *
 *    Return codes: handle to the device
 *
 **/
extern osd_dev_handle_t get_osd_dev_by_id(uint64_t osd_dev_id, int dev_lun);

/**
 *
 *    Function: put_osd_dev
 *
 *    Description:  release refernece to the @dev OSD device
 *
 *    Arguments:    @dev - handle to the OSD device
 *
 *    Return codes: None
 *
 **/
extern void put_osd_dev(osd_dev_handle_t dev);

/**
 *
 *    Function: get_osd_channel_token
 *
 *    Description:  obtain channel token from target
 *
 *    Arguments:    @dev - handle to the OSD device
 *                  @channel_token - pointer to struct to store the channel token in
 *
 *    Return codes: 0 on success
 *                  -1 on failure
 *
 **/
extern int get_osd_channel_token(osd_dev_handle_t dev,
			  osd_channel_token_t *channel_token);


/**
 *
 *    Function: get_osd_system_id
 *
 *    Description:  obtain system id from target
 *
 *    Arguments:    @dev - handle to the OSD device
 *                  @system_id - pointer to store the system id in
 *
 *    Return codes: 0 on success
 *                  -1 on failure
 *
 **/
extern int get_osd_system_id(osd_dev_handle_t dev, uint8_t system_id[20]);


/**
 *
 *    Function:     create osd context
 *
 *    Description:  allocate and initialize osd context for asynchronous
 *                  submission of OSD requests 
 *                   
 *    Arguments:    @max_nr_req - max number of request to be submitted on this
 *                  context concurrently
 *
 *    Return codes: pointer to an osd context 
 *
 **/
extern struct osd_ctx_t * create_osd_ctx(uint32_t max_nr_req);


/**
 *
 *    Function:     destroy osd context
 *
 *    Description:  free an osd context
 *                   
 *    Arguments:    @ctx - a pointer to a valid osd context.
 *                      legal value: ctx != NULL
 *
 *    Return codes: NONE 
 *
 **/
extern void destroy_osd_ctx(struct osd_ctx_t * ctx);


/**
 *
 *    Function:     prepare_osd_create_partition
 *
 *    Description:  prepare a create partition request 
 *                   
 *    Arguments:    @dev - reference to the osd device
 *                  @requested_id - requested partition id, 
 *                         0 means any partition id will be assigned.
 *
 *    Return codes: pointer to an osd request 
 *
 **/
extern struct osd_req_t * prepare_osd_create_partition(osd_dev_handle_t dev, 
                                         uint64_t requested_id);

/**
 *
 *    Function:     prepare_osd_format_lun
 *
 *    Description:  prepare a format request
 *                   
 *    Arguments:    @dev - reference to the osd device
 *                  @formatted_capacity - total capacity of lun after format.
 *                                        0 means same capacity as before format.
 *
 *    Return codes: pointer to an osd request 
 *
 **/
extern struct osd_req_t * prepare_osd_format_lun(osd_dev_handle_t dev, 
				   uint64_t formatted_capacity);

/**
 *
 *    Function:     prepare_osd_create
 *
 *    Description:  prepare a create object request 
 *                   
 *    Arguments:    @dev - reference to the osd device
 *                  @part_id - partition id
 *                  @requested_id - requested object id, 
 *                       0 means any partition id will be assigned
 *
 *    Return codes: pointer to an osd request 
 *
 **/
extern struct osd_req_t * prepare_osd_create(osd_dev_handle_t dev, 
                                             uint64_t part_id,
                                             uint64_t requested_id);

/**
 *
 *    Function:     prepare_osd_write
 *
 *    Description:  prepare a  write request 
 *                   
 *    Arguments:    @dev - reference to the osd device
 *                  @part_id - partition id
 *                  @obj_id - requested object id
 *                  @length - num of bytes to write
 *                             legal value: length <= 64K (65536)
 *                  @offset - offset in bytes to start the write
 *                  @cmd_data_use_sg - length of scatter list, 
 *                                     0 means no scatter list
 *                  @cmd_data - pointer to data buffer to be written
 *                             legal value: if length > 0 cmd_data != NULL
 *
 *    Return codes: pointer to an osd request 
 *
 **/

extern struct osd_req_t * prepare_osd_write(osd_dev_handle_t dev, 
                                            uint64_t part_id,
                                            uint64_t obj_id,
                                            uint64_t length,
                                            uint64_t offset,
                                            int cmd_data_use_sg,
                                            const char * cmd_data);

/**
 *
 *    Function:     prepare_osd_clear
 *
 *    Description:  prepare a clear request 
 *                   
 *    Arguments:    @dev - reference to the osd device
 *                  @part_id - partition id
 *                  @obj_id - requested object id
 *                  @length - num of bytes to clear
 *                  @offset - offset in bytes to start the clear
 *
 *    Return codes: pointer to an osd request 
 *
 **/
extern struct osd_req_t * prepare_osd_clear(osd_dev_handle_t dev, 
                                            uint64_t part_id,
                                            uint64_t obj_id,
                                            uint64_t length,
                                            uint64_t offset);

/**
 *
 *    Function:     prepare_osd_read
 *
 *    Description:  prepare a read request 
 *                   
 *    Arguments:    @dev - reference to the osd device
 *                  @part_id - partition id
 *                  @obj_id - requested object id
 *                  @length - num of bytes to read
 *                             legal value: length <= 64K (65536)
 *                  @offset - offset in bytes to start the read
 *                  @cmd_data_use_sg - length of scatter list, 
 *                                     0 means no scatter list
 *                  @cmd_data - pointer to data buffer to be read
 *                             legal value: if length > 0 cmd_data != NULL
 *
 *    Return codes: pointer to an osd request 
 *
 **/
extern struct osd_req_t * prepare_osd_read(osd_dev_handle_t dev, 
                                           uint64_t part_id,
                                           uint64_t obj_id,
                                           uint64_t length,
                                           uint64_t offset,
                                           int cmd_data_use_sg,
                                           char * cmd_data);

/**
 *
 *    Function:     prepare_osd_inquiry
 *
 *    Description:  prepare a inquiry request 
 *                   
 *    Arguments:    @dev - reference to the osd device
 *                  @length - num of bytes to read
 *                            legal value: length >= 16
 *                  @cmd_data - pointer to data buffer to be read
 *                            legal value: cmd_data != NULL
 *                  @page - requested VPD page (0 for standard)
 *
 *    Return codes: pointer to an osd request 
 *
 **/
extern struct osd_req_t * prepare_osd_inquiry(osd_dev_handle_t dev, 
                                              uint64_t length,
                                              char * cmd_data,
                                              uint8_t page);

/**
 *
 *    Function:     prepare_osd_list
 *
 *    Description:  prepare an OSD list request 
 *                   
 *    Arguments:    @dev - reference to the osd device
 *                  @part_id - partition id
 *                  @list_id - list identifier
 *                             0 - for new list command
 *                             previously returned list_id - for continuing old command
 *                  @alloc_len - allocaten length for retrieved list
 *                  @initial_obj_id - list will include only ids >= initial_obj_id
 *                  @use_sg - length of scatter list, 
 *                                     0 means no scatter list
 *                  @data - pointer to data buffer to be filled
 *                            legal value: if alloc_len > 0 data != NULL
 *                   
 *    Return codes: pointer to an osd request 
 *
 **/
extern struct osd_req_t * prepare_osd_list(osd_dev_handle_t dev, 
                                           uint64_t part_id,
                                           uint32_t list_id,
                                           uint64_t alloc_len,
                                           uint64_t initial_obj_id,
                                           int use_sg,
                                           char * data);

/**
 *      Function:   extract_list_from_req 
 *
 *      Purpose:    extract list of id's retrived by a list command request
 *
 *      Arguments:  @osd_req - pointer to an osd request
 *                         legal value: != NULL
 *                  @total_matches_p - output. total number of entries that would
 *                                   have been retrieved if alloc_len was infinite
 *                         legal value: != NULL
 *                  @num_ids_retrieved_p - output. num of entries in retrieved list
 *                         legal value: != NULL
 *                  @list_of_ids_p - output. address of array of 8-byte-id's
 *                         legal value: != NULL
 *                  @is_list_of_partitions_p - output. 1 if list of partitions, 
 *                                                     0 if list of user objects
 *                         legal value: != NULL
 *                  @list_isnt_up_to_date_p - output. 1 if list may have changed 
 *                                            while command was executing
 *                         legal value: != NULL
 *                  @continuation_tag_p - output. =0 if list is complete
 *                                        if nonzero, should be used as initial_id
 *                                        for another list request that would get
 *                                        rest of id's.
 *                                        (list was truncated to alloc len)
 *                         legal value: != NULL
 *                  @list_id_for_more_p - if *continuation_tag_p!=0, 
 *                                        this should be also used for the
 *                                        continuing list request as list_id param.
 *                         legal value: != NULL
 *
 *      Returns:    0 for success otherwise -1
 *
 *      Notes:      should be called for list command only after it was finished.
 **/
extern int extract_list_from_req(struct osd_req_t * req,
                                 uint64_t *total_matches_p,
                                 uint64_t *num_ids_retrieved_p,
                                 uint64_t *list_of_ids_p[],
                                 int      *is_list_of_partitions_p,
                                 int      *list_isnt_up_to_date_p,
                                 uint64_t *continuation_tag_p,
                                 uint32_t *list_id_for_more_p);


/**
 *
 *    Function:     extract_id_from_inquiry_data
 *
 *    Description:  finds the unique ID in the inquiry data
 *                   
 *    Arguments:    @data - pointer to data buffer
 *                         legal value: data != NULL
 *                   
 *    Return codes: the ID
 *
 **/
extern uint64_t extract_id_from_inquiry_data(char* data);

/**
 *
 *    Function:     prepare_osd_remove
 *
 *    Description:  prepare a remove object request 
 *                   
 *    Arguments:    @dev - reference to the osd device
 *                  @part_id - partition id
 *                  @obj_id - object id 
 *
 *    Return codes: pointer to an osd request 
 *
 **/
extern struct osd_req_t * prepare_osd_remove(osd_dev_handle_t dev, 
                                             uint64_t part_id,
                                             uint64_t obj_id);

/**
 *
 *    Function:     prepare_osd_remove_partition
 *
 *    Description:  prepare a remove partition request 
 *                   
 *    Arguments:    @dev - reference to the osd device
 *                  @part_id - requested partition id
 *
 *    Return codes: pointer to an osd request 
 *
 **/
extern struct osd_req_t * prepare_osd_remove_partition(osd_dev_handle_t dev, 
                                                       uint64_t part_id);

/**
 *
 *    Function:     prepare_osd_set_attr
 *
 *    Description:  prepare a set attribute request 
 *                   
 *    Arguments:    @dev - reference to the osd device
 *                  @part_id - partition id. 0 to access root attributes.
 *                  @obj_id - object id. 0 to acess partition/root attributes.
 *
 *    Return codes: pointer to an osd request 
 *
 **/
extern struct osd_req_t * prepare_osd_set_attr(osd_dev_handle_t dev, 
                                               uint64_t part_id,
                                               uint64_t obj_id);

/**
 *
 *    Function:     prepare_osd_get_attr
 *
 *    Description:  prepare a get attribute request 
 *
 *    Arguments:    @dev - reference to the osd device
 *                  @part_id - partition id. 0 to access root attributes.
 *                  @obj_id - object id. 0 to acess partition/root attributes.
 *
 *    Return codes: pointer to an osd request 
 *
 **/
extern struct osd_req_t * prepare_osd_get_attr(osd_dev_handle_t dev, 
                                               uint64_t part_id,
                                               uint64_t obj_id);

/**
 *
 *    Function:     prepare_osd_set_key
 *
 *    Description:  prepare a SET KEY request
 *                   
 *    Arguments:    @dev - reference to the osd device
 *                  @key_to_set - key type code according to standard
 *                                (3=working key 2=partition key 1=drive key)
 *                  @part_id - partition id
 *                  @key_ver - for working key only, should be 0 otherwise
 *                  @key_id - key name - in ascii characters.
 *                         legal value: key_id != NULL
 *                  @seed - seed used in command
 *                         legal value: seed != NULL
 *
 *    Return codes: pointer to an osd request 
 *
 **/
extern struct osd_req_t * prepare_osd_set_key(osd_dev_handle_t dev, 
                                              uint8_t key_to_set,
                                              uint64_t part_id,
                                              uint8_t key_ver,
                                              uint8_t key_id[7],
                                              uint8_t seed[20]);

/**
 *
 *    Function:     prepare_osd_debug
 *
 *    Description:  prepare a DEBUG COMMAND request
 *                   
 *    Arguments:    @dev - reference to the osd device
 *                  @data_out_len - length of debug data buf sent by initiator
 *                  @data_in_len  - length of debug data to receive from target
 *                  @data_out -  buffer to send to target
 *                  @data_in  - buffer for data received from target
 *
 *    Return codes: pointer to an osd request 
 *
 **/
extern struct osd_req_t * prepare_osd_debug(osd_dev_handle_t dev, 
                                            uint32_t data_out_len,
                                            uint32_t data_in_len,
                                            char * data_out,
                                            char * data_in);

/********************************************
 *    Build set and get attribute params
 ********************************************/

/**
 *
 *    Function:     prepare_get_attr_list_add_entry
 *
 *    Description:  add a get attribute request to the list of get attributes 
 *                   
 *    Arguments:    @req - reference to the osd request
 *                      legal value: req != NULL
 *                  @page_num - attribute page number
 *                  @attr_num - attribute number
 *                  @get_attr_length - length of attribute value to be 
 *                                     retrieved
 *
 *    Return codes: 0 for success otherwise -1 
 *
 *    Notes:        Numeric attributes values are kept in network byte order 
 *                  (big endian). For such attributes, the received value 
 *                  should be converted to the local convention before being used.
 *
 **/
extern int prepare_get_attr_list_add_entry( struct osd_req_t * req,
                                            uint32_t page_num,
                                            uint32_t attr_num,
                                            uint32_t retrieved_attr_length);


/**
 *
 *    Function:     prepare_set_attr_list_add_entry
 *
 *    Description:  add a set attribute request to the list of 
 *                  set attributes 
 *                   
 *    Arguments:    @req - reference to the osd request
 *                      legal value: req != NULL
 *                  @page_num - attribute page number
 *                  @attr_num - attribute number
 *                  @attr_len - length of attr_val to be set 
 *                      leagl value: attr_len < OSD_MAX_SET_LIST_LEN-4
 *                  @attr_val - pointer to attributes value
 *                      legal value: if attr_len > 0 attr_val != NULL
 *
 *    Return codes: 0 for success otherwise -1 
 *
 *    Notes:        Numeric attributes values are kept in network byte order 
 *                  (big endian). For such attributes, values should be converted
 *                  before setting them.
 * 
 **/
extern int prepare_set_attr_list_add_entry( struct osd_req_t * req,
                                            uint32_t page_num,
                                            uint32_t attr_num,
                                            uint16_t attr_len,
                                            const unsigned char *attr_val);

/**
 *
 *    Function:     prepare_set_timestamp_bypass_mode
 *
 *    Description:  specify timestamps bypass mode for this request
 *                   
 *    Arguments:    @req - reference to the osd request
 *                      legal value: req != NULL
 *                  @bypass_mode - mode value to set
 *
 *    Return codes: 0 for success otherwise -1
 *
 **/
extern int prepare_set_timestamp_bypass_mode(struct osd_req_t *req,uint8_t bypass_mode);

/***************************************************
 *            Submission of OSD request 
 ***************************************************/

/**
 *      Function:    do_osd_req_timeout 
 *
 *      Purpose:     execute an osd request invoke done on completion
 *
 *      Arguments:   @osd_req - pointer to a prepared osd request
 *                       legal value: osd_req != NULL
 *                   @ctx - pointer to osd context to submit request on.
 *                           in kernel mode ctx should be NULL
 * 	             @done - function pointer to be invoked on completion
 *                       legal value: done != NULL
 *                   @caller_context - caller private context 2nd arg to done
 *                                     callback
 *                   @timeout - timeout for command execution in seconds
 *                   @credential - full credential to use for command authentication
 *                   @channel_token - channel token to use for command authentication
 *
 *      Returns:     status of queueing - 0 for success other value for failure
 *                   -EINTR is returned if a system call was interrupted.
 *
 *      Notes:       Used for read, write, all OSD ops (incl. SET_KEY etc.)
 *                   In case an error value is returned (!=0) the callback function
 *                   will not be called. Otherwise, callback will always be 
 *                   on separate thread
 *
 **/
extern int do_osd_req_timeout(struct osd_req_t * osd_req, 
                              struct osd_ctx_t * ctx, 
                              void (* done) (struct osd_req_t *, void *), 
                              void * caller_context,
                              unsigned int timeout, 
                              uint8_t credential[OSD_CREDENTIAL_SIZE],
                              osd_channel_token_t *channel_token);


/**
 *      Function:    do_osd_req 
 *
 *      Purpose:     execute an osd request invoke done on completion
 *
 *      Arguments:   @osd_req - pointer to a prepared osd request
 *                       legal value: osd_req != NULL
 *                   @ctx - pointer to osd context to submit request on 
 *                          in kernel mode, ctx should be NULL
 * 	             @done - function pointer to be invoked on completion
 *                       legal value: done != NULL
 *                   @caller_context - caller private context 2nd arg to done
 *                                     callback
 *                   @credential - full credential to use for command authentication
 *                   @channel_token - channel token to use for command authentication
 *
 *      Returns:     status of queueing - 0 for success other value for failure
 *                   -EINTR is returned if a system call was interrupted.
 *
 *      Notes:       Used for read, write, all OSD ops (incl. SET_KEY etc.)
 *                   In case an error value is returned (!=0) the callback function
 *                   will not be called. Otherwise, callback will always be 
 *                   on separate thread
 **/
extern int do_osd_req(struct osd_req_t * osd_req, 
                      struct osd_ctx_t * ctx, 
                      void (* done) (struct osd_req_t *, void *), 
                      void * caller_context,
                      uint8_t credential[OSD_CREDENTIAL_SIZE],
                      osd_channel_token_t *channel_token);


/**
 *      Function:   get_completed_osd_req
 *
 *      Purpose:    get notification for all completed osd request under @ctx
 *                  in user mode applications.
 *
 *      Arguments:  @ctx - pointer to context where request where submit on.
 *                  @min_nr - minimu number of request completed before 
 *                  returning to caller.
 *                  @nr - max number of osd requests to be notified about 
 *                  completion under this call.
 *
 *      Returns:    if rc>=0 rc is the number of completed requests
 *                  if rc <0 , it is an error code.
 *                  rc == -EINTR return value means a system call was interrupted.
 *                  0< rc <min_nr may be returned when a system call was interrupted
 *                  or when OSD_DEFAULT_TIMEOUT seconds have passed.
 *
 *      Notes:      completion of osd requests will be done by calling the
 *                  requests callbaclk before this function returns to 
 *                  the user.
 *                  In kernel mode this api has no purpose.
 *                  
 **/
extern int get_completed_osd_req(struct osd_ctx_t * ctx, int32_t min_nr, int32_t nr);

/**
 *      Function:   wait_osd_req 
 *
 *      Purpose:    execute osd request block until completion
 *
 *      Arguments:  @osd_req - pointer to a prepared osd request
 *                  @timeout - duration of timeout in "jiffies"
 *                  @retries - number of retries we allow
 *                  @credential - full credential to use for command authentication
 *                  @channel_token - channel token to use for command authentication
 *
 *      Returns:    rc == 0 on success
 *                  rc < 0 if operation failed before osd_req was sent.
 *                  e.g. rc == -EINTR if interrupted before sending osd_req
 *                  rc > 0 if operation failed after osd_req was sent
 *                  e.g. rc==EINTR  if interrupted while waiting for completion
 *    
 **/
extern int wait_osd_req(struct osd_req_t * osd_req, 
                        unsigned int timeout, unsigned int retries,
                        uint8_t credential[OSD_CREDENTIAL_SIZE],
                        osd_channel_token_t *channel_token);

/**
 *      Function:   free_osd_req 
 *
 *      Purpose:    free an osd request
 *
 *      Arguments:  @osd_req - pointer to an osd request
 *
 *      Returns:    Nothing
 *
 *      Notes:      must be called after IO completed
 **/
extern void free_osd_req(struct osd_req_t * osd_req);

/*************************************************************
 *     API for attribute extraction from completed OSD request
 *************************************************************/

/**
 *      Function:   init_extract_attr_from_req 
 *
 *      Purpose:    init osd request control block before extracting 
 *                  attributes
 *
 *
 *      Arguments:  @osd_req - pointer to an osd request
 *                      legal value: osd_req != NULL
 *
 *      Returns:    0 for success otherwise -1
 *
 *      Notes:      must be called after IO completed
 **/
extern int init_extract_attr_from_req(struct osd_req_t * osd_req);

/**
 *      Function:   extract_next_attr_from_req 
 *
 *      Purpose:    extract next attribute retrieved by req
 *
 *      Arguments:  @osd_req - pointer to an osd request
 *                      legal value: osd_req != NULL
 *                  @page_num - pointer to page number of attribute 
 *                              retrieved
 *                  @attr_num - poiter to attribute number retrieved
 *                  @attr_len - pointer to length of attribute retrieved
 *                  @attr_val - pointer to value of attribute retrieved
 *
 *      Returns:    0 for success otherwise -1
 *
 *      Notes:      - must be called after init_extract_attr_from_req
 *
 *                  - Numeric attributes values are kept in network byte order 
 *                  (big endian). For such attributes, the extracted value 
 *                  should be converted to the local convention before being used.
 **/
extern int extract_next_attr_from_req(struct osd_req_t * req, uint32_t * page_num, uint32_t * attr_num, uint16_t * attr_len, uint8_t ** attr_val);

extern int osd_get_result(struct osd_req_t * req, osd_result_t * res_o);

/**
 *      Function:   osd_get_sense 
 *
 *      Purpose:    retrieve sense from a request ended with 
 *                  OSD_CHECK_CONDITION status 
 *
 *      Arguments:  @osd_req - pointer to an osd request
 *                  @sense_buf - buffer fot retrieving sense data
 *                  @sense_len - lengeth of sense_buf in bytes
 *
 *                  is the size of the returned sense_buf fixed? if not, sense_len should be modified by osd_get_sense 
 *
 *      Returns:    0 on success, -1 on failure
 *
 *
 **/
extern int osd_get_sense(struct osd_req_t * osd_req, unsigned char * sense_buf, uint32_t sense_len);

/**
 *      Function:   get_osd_load 
 *
 *      Purpose:    get the number of operations currently being handled by the
 *                  initiator (technically speaking, how many OSD request
 *                  structs are allocated at this point)
 *      Returns:    Number of commands in progress on success
 *                  -1 on failure
 **/
extern int get_osd_load(void);

extern void osd_req_module_init(void);

#endif /* OSD_REQ_H */
