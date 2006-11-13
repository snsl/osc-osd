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
#ifndef OSD_REQ_IMP_H
#define OSD_REQ_IMP_H

#include "osd_cdb.h"
#include "osd_cdb_cmd.h"
#include "osd_attr.h"
#include "bidi_buffer.h"

#ifdef __KERNEL__
#include <linux/blkdev.h>
#include <scsi/scsi_request.h>
#include <scsi.h>
#include <linux/uio.h>
#else
#include <errno.h> 
#endif

#define OSD_MAX_DATA_LEN 1000000

#define OSD_MAX_GET_LIST_LEN 512
#define OSD_MAX_SET_LIST_LEN 512

#define MAX_IOCB_PER_REQ 1

#ifndef __KERNEL__
#define MAX_COMMAND_SIZE  200
#endif

typedef struct osd_scsi_desc_t {
    uint16_t cdb_len;
    unsigned char cdb[MAX_COMMAND_SIZE];
    int use_sg_out;
    int data_out_len;
    unsigned char * data_out;
    int use_sg_in;
    int data_in_len;
    unsigned char * data_in;

    int sense_len;
    unsigned char sense_buf[OSD_SENSE_MAX_LEN];
    unsigned char * sense_ptr;
    
    int timeout;
    int max_retries;
} osd_scsi_desc_t; 

typedef enum osd_get_set_attr_fmt_t {
    OSD_GET_SET_INVALID_FMT = 0,
    OSD_GET_SET_LIST_FMT,
    OSD_GET_SET_PAGE_FMT,
    OSD_GET_SET_NR
} osd_get_set_attr_fmt_t;

typedef union cmd_params_union_t {
    struct {
	uint64_t requested_id;
    } create_partition;
    struct {
	uint64_t part_id;
	uint64_t requested_id;
	uint16_t num_objects;
    } create;
    struct {
	uint64_t part_id;
	uint64_t obj_id;
	uint64_t length;
	uint64_t offset;
    } write;
    struct {
	uint64_t part_id;
	uint64_t obj_id;
	uint64_t length;
	uint64_t offset;
    } clear;
    struct {
	uint64_t part_id;
	uint64_t obj_id;
	uint64_t length;
	uint64_t offset;
    } read;
    struct {
	uint64_t part_id;
	uint64_t obj_id;
    } remove;
    struct {
	uint64_t part_id;
    } remove_partition;
    struct {
	uint64_t part_id;
	uint64_t obj_id;
    } set_attr;
    struct {
	uint64_t part_id;
	uint64_t obj_id;
    } get_attr;
    struct {
	uint64_t part_id;
	uint32_t list_id;
	uint64_t alloc_len;
	uint64_t initial_obj_id;
    } list;

    struct {
	uint64_t formatted_capacity;
    } format;

    struct {
	uint64_t part_id;
	uint8_t key_to_set;
	uint8_t key_ver;
	uint8_t key_id_a[7];
	uint8_t seed_a[20];
    } set_key;
    struct {
	uint32_t data_out_len;
	uint32_t data_in_len;
    } debug;
} osd_cmd_params_t;

#ifdef __KERNEL__

static inline void * os_malloc(int size)
{
    return kmalloc(size, GFP_KERNEL);
}

static inline void os_free(void * ptr)
{
    kfree(ptr);
}

typedef struct os_completion_t {
    uint8_t completed;
    struct completion completion;
}os_completion_t;

static inline void os_init_completion(os_completion_t * completion)
{
    completion->completed = 0;
    init_completion(&completion->completion);
}

static inline void os_wait_for_completion(os_completion_t * completion)
{
    if(completion->completed == 0) {
        wait_for_completion(&completion->completion);
    }
}

static inline void os_complete(os_completion_t * completion)
{
    completion->completed = 1;
    complete(&completion->completion);
}

typedef struct osd_ctx_t {
} osd_ctx_t;

typedef struct osd_req_t {
    struct list_head list;
    
    osd_dev_handle_t dev;

    /* osd requst's arguments */
    uint16_t service_action;
    
    union cmd_params_union_t cmd_params;
    
    /* data structures for get/set attribute on DATA OUT buffer */ 
    uint16_t new_get_list_len;
    uint8_t * new_get_list_buf;
    uint16_t new_set_list_len;
    uint8_t * new_set_list_buf;
    
    attr_list_desc_t get_list_desc;
    attr_list_desc_t set_list_desc;
    
    osd_scsi_desc_t scsi_desc;
    int cmd_data_use_sg;
    uint32_t cmd_data_len;
    unsigned char * cmd_data;
    int cmd_data_direction;
    
    int sense_sg_len;
    struct scatterlist sense_sg[2];

    /* static iovec and sg for write commands */
    struct iovec data_out_iov[64];
    struct scatterlist data_out_sg[128];
    
    uint8_t is_user_addr;
    uint8_t allocated_data_out;
    uint8_t allocated_data_in;
    void * allocated_sg_out;
    void * allocated_sg_in;
    struct iovec * iov_out;
    struct iovec * iov_in;

    uint32_t set_attribute_list_offset;
    uint32_t get_attribute_list_offset;
    uint32_t retrieved_attr_offset;
    uint32_t retrieved_attr_len;
    uint8_t allocated_retrieved_attr_buf;
    unsigned char * retrieved_attr_buf;
    osd_get_set_attr_fmt_t get_set_attr_fmt;

    uint8_t timestamp_ctl;

    struct bidi_buffer bidi_buf;
    
    void (* done) (struct osd_req_t *, void *);
    void * caller_context;
    int status;
    int non_osd_cmd;
} osd_req_t;


#else
#include <sys/uio.h>
#include <scsi/scsi.h>
#include <libaio.h>

#ifndef DMA_BIDIRECTIONAL
#define DMA_BIDIRECTIONAL 0
#endif

#ifndef DMA_TO_DEVICE
#define DMA_TO_DEVICE 1
#endif

#ifndef DMA_FROM_DEVICE
#define DMA_FROM_DEVICE 2
#endif

#ifndef DMA_NONE
#define DMA_NONE 3
#endif

#include <pthread.h>

struct osd_req_t;

typedef struct osd_ctx_t {
    io_context_t io_ctx;
    struct osd_req_t *reqs;
} osd_ctx_t;

typedef struct osd_req_t {
    char link[8];  // This used by the harness as a linked-list
    osd_dev_handle_t dev;

    /* osd requst's arguments */
    uint16_t service_action;

    union cmd_params_union_t cmd_params;
    
    /* data structures for get/set attribute on DATA OUT buffer */ 
    uint16_t new_get_list_len;
    uint8_t * new_get_list_buf;
    uint16_t new_set_list_len;
    uint8_t * new_set_list_buf;
    
    attr_list_desc_t get_list_desc;
    attr_list_desc_t set_list_desc;
    
    int cmd_data_use_sg;
    uint32_t cmd_data_len;
    unsigned char * cmd_data;
    int cmd_data_direction;    

    osd_scsi_desc_t scsi_desc;
    unsigned char * allocated_pad;
    uint8_t allocated_data_in;
    uint8_t allocated_data_out;
    
    uint32_t set_attribute_list_offset;
    uint32_t get_attribute_list_offset;
    uint32_t retrieved_attr_offset;
    uint32_t retrieved_attr_len;
    uint8_t allocated_retrieved_attr_buf;
    unsigned char * retrieved_attr_buf;
    osd_get_set_attr_fmt_t get_set_attr_fmt;

    uint8_t timestamp_ctl;
    
    struct iocb * iocb[MAX_IOCB_PER_REQ];
    int iocb_len;
    
    void * scsi_request;

    void (* done) (struct osd_req_t *, void *);
    void * caller_context;
    int status;
    int non_osd_cmd;
    struct osd_req_t *next;
} osd_req_t;

static inline void * os_malloc(int size)
{
    return malloc(size);
}

static inline void os_free(void * ptr)
{
    free(ptr);
}

typedef struct os_completion_t {
    uint8_t completed;
    pthread_cond_t cond;
    pthread_mutex_t mutex;
} os_completion_t;

static inline void os_init_completion(os_completion_t * completion)
{
    completion->completed = 0;
    pthread_mutex_init (&completion->mutex, NULL);
    pthread_cond_init(&completion->cond, NULL);
}

static inline void os_wait_for_completion(os_completion_t * completion)
{
    pthread_mutex_lock(&completion->mutex);
    if(completion->completed == 0) {
        pthread_cond_wait(&completion->cond, &completion->mutex);
    }
    pthread_mutex_unlock(&completion->mutex);
}

static inline void os_complete(os_completion_t * completion)
{
    pthread_mutex_lock(&completion->mutex);
    if(completion->completed == 0) {
        completion->completed = 1;
        pthread_cond_signal(&completion->cond);
    }
    pthread_mutex_unlock(&completion->mutex);
}

#endif /* __KERNEL__ */
/* platform dependent stuff */

extern uint32_t g_osd_trace_exclude,g_osd_trace_include;

/*  wait_osd_via_scsi:
 *      Returns:    rc == 0 on success
 *                  rc < 0 if operation failed before osd_req was sent.
 *                  e.g. rc == -EINTR if interrupted before sending osd_req
 *                  rc > 0 if operation failed after osd_req was sent
 *                  e.g. rc==EINTR  if interrupted while waiting for completion
 */
int wait_osd_via_scsi(osd_req_t * req, unsigned int timeout, unsigned int retries);

/*  create_osd_ctx_via_scsi:
 *      Returns:    NULL on failure, non-NULL context on success
 */
osd_ctx_t * create_osd_ctx_via_scsi(uint32_t max_nr_req);

void destroy_osd_ctx_via_scsi(osd_ctx_t * ctx);


/*  do_osd_via_scsi : 
 *  Returns:     status of queueing - 0 for success other value for failure
 *               -EINTR is returned if a system call was interrupted.
 */
int do_osd_via_scsi(osd_req_t * req, osd_ctx_t * ctx, void (* done) (osd_req_t *, void *), void * caller_context);

/* get_completed_osd_req_via_scsi:
 *      Returns:    if rc>=0 rc is the number of completed requests
 *                  if rc <0 , it is an error code.
 *                  rc == -EINTR return value means a system call was interrupted.
 *                  0< rc <min_nr may be returned when a system call was interrupted
 *                  or when OSD_DEFAULT_TIMEOUT seconds have passed.
 */
int get_completed_osd_req_via_scsi(osd_ctx_t * ctx, int32_t min_nr, int32_t nr);

osd_req_t * os_alloc_osd_req(void);
void os_free_osd_req(osd_req_t * req);


#endif
