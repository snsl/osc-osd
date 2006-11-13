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
#ifndef OSD_OPS_H
#define OSD_OPS_H

#include "osd_req.h"

int osd_create_partition(osd_dev_handle_t dev, uint64_t *part_id);

int osd_create_object(osd_dev_handle_t dev, uint64_t part_id, uint64_t * oid);

int osd_format(osd_dev_handle_t dev, uint64_t tot_capacity);

int osd_clear(osd_dev_handle_t dev, 
              uint32_t part_id, 
              uint64_t oid, 
              uint64_t offset, 
              uint64_t len);

int osd_write(osd_dev_handle_t dev, 
              uint32_t part_id, 
              uint64_t oid, 
              const char * data_out, 
              uint64_t offset, 
              uint64_t len, 
              uint64_t * obj_len);

int osd_append(osd_dev_handle_t dev, 
               uint32_t part_id, 
               uint64_t oid, 
               unsigned char * data_out, 
               uint64_t len, 
               uint64_t * obj_len);

int osd_create_n_wr(osd_dev_handle_t dev, 
                    uint64_t part_id, 
                    uint64_t *oid, 
                    unsigned char * data_out, 
                    uint64_t offset, 
                    uint64_t len, 
                    uint64_t * obj_len);

int osd_read(osd_dev_handle_t dev, 
             uint64_t part_id, 
             uint64_t oid,
             uint64_t offset,
             char * data_in, 
             uint64_t len);

int osd_list(osd_dev_handle_t dev, 
             uint64_t part_id, 
             char * data_in,
             uint32_t *tot_entries,
             uint64_t *list_arr[],
             uint64_t len);

int osd_remove(osd_dev_handle_t dev, uint64_t part_id, uint64_t oid);

int osd_remove_partition(osd_dev_handle_t dev, uint64_t part_id);

int osd_set_one_attr(osd_dev_handle_t dev, 
                     uint64_t part_id, 
                     uint64_t oid, 
                     uint32_t page, 
                     uint32_t attr, 
                     uint32_t len, 
                     void * value);

int osd_get_one_attr(osd_dev_handle_t dev, uint64_t part_id, uint64_t oid, uint32_t page, uint32_t attr, uint32_t alloc_len, uint16_t * len, void * value);

int osd_inquiry_83(osd_dev_handle_t dev, char * data_in, uint64_t len);

int osd_set_root_key(osd_dev_handle_t dev, 
		     uint8_t seed_a[OSD_CRYPTO_VAL_SIZE],
		     uint8_t new_key_id_a[7]);

#endif /* OSD_OPS_H */
