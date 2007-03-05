#ifndef _CDB_MANIP_H
#define _CDB_MANIP_H

#include "kernel_interface.h"

struct attribute_id {
        uint32_t page;
        uint32_t number;
        uint16_t len;
	void *tag;
};

int osd_command_set_inquiry(struct osd_command *command, uint8_t outlen);

int osd_command_set_append(struct osd_command *command, uint64_t pid, uint64_t oid, uint64_t len);


int osd_command_set_create(struct osd_command *command, uint64_t pid, uint64_t requested_oid, uint16_t num_user_objects);


int osd_command_set_create_and_write(struct osd_command *command, uint64_t pid, uint64_t requested_oid,
                         uint64_t len, uint64_t offset);


int osd_command_set_create_collection(struct osd_command *command, uint64_t pid, uint64_t requested_cid);

int osd_command_set_create_partition(struct osd_command *command, uint64_t requested_pid);


int osd_command_set_flush(struct osd_command *command, uint64_t pid, uint64_t oid, int flush_scope);


int osd_command_set_flush_collection(struct osd_command *command, uint64_t pid, uint64_t cid,
                         int flush_scope);


int osd_command_set_flush_osd(struct osd_command *command, int flush_scope);


int osd_command_set_flush_partition(struct osd_command *command, uint64_t pid, int flush_scope);

int osd_command_set_format_osd(struct osd_command *command, uint64_t capacity);


int osd_command_set_get_attributes(struct osd_command *command, uint64_t pid, uint64_t oid);


int osd_command_set_get_member_attributes(struct osd_command *command, uint64_t pid, uint64_t cid);


int osd_command_set_list(struct osd_command *command, uint64_t pid, uint32_t list_id, uint64_t alloc_len,
             uint64_t initial_oid);

int osd_command_set_list_collection(struct osd_command *command, uint64_t pid, uint64_t cid,
                        uint32_t list_id, uint64_t alloc_len,
                        uint64_t initial_oid);

int osd_command_set_perform_scsi_command(struct osd_command *command);

int osd_command_set_perform_task_mgmt_func(struct osd_command *command);

int osd_command_set_query(struct osd_command *command, uint64_t pid, uint64_t cid, uint32_t query_len,
              uint64_t alloc_len);

int osd_command_set_read(struct osd_command *command, uint64_t pid, uint64_t oid, uint64_t len,
             uint64_t offset);


int osd_command_set_remove(struct osd_command *command, uint64_t pid, uint64_t oid);


int osd_command_set_remove_collection(struct osd_command *command, uint64_t pid, uint64_t cid);


int osd_command_set_remove_member_objects(struct osd_command *command, uint64_t pid, uint64_t cid);


int osd_command_set_remove_partition(struct osd_command *command, uint64_t pid);


int osd_command_set_set_attributes(struct osd_command *command, uint64_t pid, uint64_t oid);


int osd_command_set_set_key(struct osd_command *command, int key_to_set, uint64_t pid, uint64_t key,
                uint8_t seed[20]);


int osd_command_set_set_master_key(struct osd_command *command, int dh_step, uint64_t key,
                       uint32_t param_len, uint32_t alloc_len);


int osd_command_set_set_member_attributes(struct osd_command *command, uint64_t pid, uint64_t cid);


int osd_command_set_write(struct osd_command *command, uint64_t pid, uint64_t oid, uint64_t len,
              uint64_t offset);


void set_cdb_get_attr_page(struct osd_command *command, uint32_t page, uint32_t len,
                           uint32_t retrieved_offset);

void set_cdb_get_attr_list(struct osd_command *command, uint32_t list_len,
                           uint32_t list_offset, uint32_t alloc_len,
                           uint32_t retrieved_offset);

int osd_command_attr_build(struct osd_command *command,
                           struct attribute_id *attrs, int num);

uint8_t *osd_command_attr_resolve(struct osd_command *command,
                                  struct attribute_id *attrs, int num,
			          int index);

#endif
