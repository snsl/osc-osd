#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <scsi/sg.h>

#include <stdint.h>
#include <sys/types.h>

#include "util/util.h"

#include "cdb_manip.h"
#include "osd_cmds.h"

static void set_action(uint8_t *cdb, uint16_t command)
{
        cdb[8] = (command & 0xff00U) >> 8;
        cdb[9] = (command & 0x00ffU);
}

/*
 * These functions take a cdb of the appropriate size (200 bytes),
 * and the arguments needed for the particular command.  They marshal
 * the arguments but do not submit the command.
 */     
int set_cdb_osd_append(uint8_t *cdb, uint64_t pid, uint64_t oid, uint64_t len)
{
        debug(__func__);
        set_action(cdb, OSD_APPEND);
        set_htonll(&cdb[16], pid);
        set_htonll(&cdb[24], oid);
        set_htonll(&cdb[36], len);
        return 0;
}


int set_cdb_osd_create(uint8_t *cdb, uint64_t pid, uint64_t requested_oid, uint16_t num)
{
        debug(__func__);
        set_action(cdb, OSD_CREATE);
        set_htonll(&cdb[16], pid);
        set_htonll(&cdb[24], requested_oid);
        set_htons(&cdb[36], num); /* Number of user objects. */
        return 0;
}


int set_cdb_osd_create_and_write(uint8_t *cdb, uint64_t pid, uint64_t requested_oid,
                         uint64_t len, uint64_t offset)
{
        debug(__func__);
        set_action(cdb, OSD_CREATE_AND_WRITE);
        set_htonll(&cdb[16], pid);
        set_htonll(&cdb[24], requested_oid);
        set_htonll(&cdb[36], len);
        set_htonll(&cdb[44], offset);
        return 0;
}


int set_cdb_osd_create_collection(uint8_t *cdb, uint64_t pid, uint64_t requested_cid)
{
        debug(__func__);
        set_action(cdb, OSD_CREATE_COLLECTION);
        set_htonll(&cdb[16], pid);
        set_htonll(&cdb[24], requested_cid);
        return 0;
}

int set_cdb_osd_create_partition(uint8_t *cdb, uint64_t requested_pid)
{
        debug(__func__);
        set_action(cdb, OSD_CREATE_PARTITION);
        set_htonll(&cdb[16], requested_pid);
        return 0;
}


int set_cdb_osd_flush(uint8_t *cdb, uint64_t pid, uint64_t oid, int flush_scope)
{
        debug(__func__);
        set_action(cdb, OSD_FLUSH);
        set_htonll(&cdb[16], pid);
        set_htonll(&cdb[24], oid);
        cdb[10] = (cdb[10] & ~0x3) | flush_scope;
        return 0;
}


int set_cdb_osd_flush_collection(uint8_t *cdb, uint64_t pid, uint64_t cid,
                         int flush_scope)
{
        debug(__func__);
        set_action(cdb, OSD_FLUSH_COLLECTION);
        set_htonll(&cdb[16], pid);
        set_htonll(&cdb[24], cid);
        cdb[10] = (cdb[10] & ~0x3) | flush_scope;
        return 0;
}


int set_cdb_osd_flush_osd(uint8_t *cdb, int flush_scope)
{
        debug(__func__);
        set_action(cdb, OSD_FLUSH_OSD);
        cdb[10] = (cdb[10] & ~0x3) | flush_scope;
        return 0;
}


int set_cdb_osd_flush_partition(uint8_t *cdb, uint64_t pid, int flush_scope)
{
        debug(__func__);
        set_action(cdb, OSD_FLUSH_PARTITION);
        set_htonll(&cdb[16], pid);
        cdb[10] = (cdb[10] & ~0x3) | flush_scope;
        return 0;
}

/*
 * Destroy the db and start over again.
 */
int set_cdb_osd_format_osd(uint8_t *cdb, uint64_t capacity)
{
        debug(__func__);
        set_action(cdb, OSD_FORMAT_OSD);
        set_htonll(&cdb[36], capacity);
        return 0;
}


int set_cdb_osd_get_attributes(uint8_t *cdb, uint64_t pid, uint64_t oid)
{
        debug(__func__);
        set_action(cdb, OSD_GET_ATTRIBUTES);
        set_htonll(&cdb[16], pid);
        set_htonll(&cdb[24], oid);
        return 0;
}


int set_cdb_osd_get_member_attributes(uint8_t *cdb, uint64_t pid, uint64_t cid) /*section in spec?*/
{
        debug(__func__);
        set_action(cdb, OSD_GET_MEMBER_ATTRIBUTES);
        set_htonll(&cdb[16], pid);
        set_htonll(&cdb[24], cid);
        return 0;
}


int set_cdb_osd_list(uint8_t *cdb, uint64_t pid, uint32_t list_id, uint64_t alloc_len,
             uint64_t initial_oid)
{
        debug(__func__);
        set_action(cdb, OSD_LIST);
        set_htonll(&cdb[16], pid);
        set_htonl(&cdb[32], list_id);
        set_htonll(&cdb[36], alloc_len);
        set_htonll(&cdb[44], initial_oid);
        return 0;
}

int set_cdb_osd_list_collection(uint8_t *cdb, uint64_t pid, uint64_t cid,  /*section in spec?*/
                        uint32_t list_id, uint64_t alloc_len,
                        uint64_t initial_oid)
{
        debug(__func__);
        set_action(cdb, OSD_LIST_COLLECTION);
        set_htonll(&cdb[16], pid);
        set_htonll(&cdb[24], cid);
        set_htonl(&cdb[32], list_id);
        set_htonll(&cdb[36], alloc_len);
        set_htonll(&cdb[44], initial_oid);
        return 0;
}

int set_cdb_osd_perform_scsi_command(uint8_t *cdb)
{
        error("%s: unimplemented", __func__);
        return 1;
}

int set_cdb_osd_perform_task_mgmt_func(uint8_t *cdb)
{
        error("%s: unimplemented", __func__);
        return 1;
}

int set_cdb_osd_query(uint8_t *cdb, uint64_t pid, uint64_t cid, uint32_t query_len,    /*section in spec?*/
              uint64_t alloc_len)
{
        debug(__func__);
        set_action(cdb, OSD_QUERY);
        set_htonll(&cdb[16], pid);
        set_htonll(&cdb[24], cid);
        set_htonl(&cdb[32], query_len);
        set_htonll(&cdb[36], alloc_len);
        return 0;
}

int set_cdb_osd_read(uint8_t *cdb, uint64_t pid, uint64_t oid, uint64_t len,
             uint64_t offset)
{
        debug(__func__);
        set_action(cdb, OSD_READ);
        set_htonll(&cdb[16], pid);
        set_htonll(&cdb[24], oid);
        set_htonll(&cdb[36], len);
        set_htonll(&cdb[44], offset);
        return 0;
}


int set_cdb_osd_remove(uint8_t *cdb, uint64_t pid, uint64_t oid)
{
        debug(__func__);
        set_action(cdb, OSD_REMOVE);
        set_htonll(&cdb[16], pid);
        set_htonll(&cdb[24], oid);
        return 0;
}


int set_cdb_osd_remove_collection(uint8_t *cdb, uint64_t pid, uint64_t cid)
{
        debug(__func__);
        set_action(cdb, OSD_REMOVE_COLLECTION);
        set_htonll(&cdb[16], pid);
        set_htonll(&cdb[24], cid);
        return 0;
}


int set_cdb_osd_remove_member_objects(uint8_t *cdb, uint64_t pid, uint64_t cid) /*section in spec?*/
{
        debug(__func__);
        set_action(cdb, OSD_REMOVE_MEMBER_OBJECTS);
        set_htonll(&cdb[16], pid);
        set_htonll(&cdb[24], cid);
        return 0;
}


int set_cdb_osd_remove_partition(uint8_t *cdb, uint64_t pid)
{
        debug(__func__);
        set_action(cdb, OSD_REMOVE_PARTITION);
        set_htonll(&cdb[16], pid);
        return 0;
}


int set_cdb_osd_set_attributes(uint8_t *cdb, uint64_t pid, uint64_t oid)
{
        debug(__func__);
        set_action(cdb, OSD_SET_ATTRIBUTES);
        set_htonll(&cdb[16], pid);
        set_htonll(&cdb[24], oid);
        return 0;
}


int set_cdb_osd_set_key(uint8_t *cdb, int key_to_set, uint64_t pid, uint64_t key,
                uint8_t seed[20])
{
        debug(__func__);
        set_action(cdb, OSD_SET_KEY);
        cdb[11] = (cdb[11] & ~0x3) | key_to_set;
        set_htonll(&cdb[16], pid);
        set_htonll(&cdb[24], key);
        memcpy(&cdb[32], seed, 20);
        return 0;
}


int set_cdb_osd_set_master_key(uint8_t *cdb, int dh_step, uint64_t key,
                       uint32_t param_len, uint32_t alloc_len)
{
        debug(__func__);
        set_action(cdb, OSD_SET_KEY);
        cdb[11] = (cdb[11] & ~0x3) | dh_step;
        set_htonll(&cdb[24], key);
        set_htonl(&cdb[32], param_len);
        set_htonl(&cdb[36], alloc_len);
        return 0;
}


int set_cdb_osd_set_member_attributes(uint8_t *cdb, uint64_t pid, uint64_t cid)
{
        debug(__func__);
        set_action(cdb, OSD_SET_MEMBER_ATTRIBUTES);
        set_htonll(&cdb[16], pid);
        set_htonll(&cdb[24], cid);
        return 0;
}


int set_cdb_osd_write(uint8_t *cdb, uint64_t pid, uint64_t oid, uint64_t len,
              uint64_t offset)
{
        debug(__func__);
        set_action(cdb, OSD_WRITE);
        set_htonll(&cdb[16], pid);
        set_htonll(&cdb[24], oid);
        set_htonll(&cdb[36], len);
        set_htonll(&cdb[44], offset);
        return 0;
}
