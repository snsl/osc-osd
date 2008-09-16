#ifndef __OBJ_H
#define __OBJ_H

#include <sqlite3.h>

int obj_insert(sqlite3 *db, uint64_t pid, uint64_t oid, uint32_t type);
int obj_delete(sqlite3 *db, uint64_t pid, uint64_t oid);
int obj_get_nextoid(sqlite3 *db, uint64_t pid, uint64_t *oid);
int obj_get_nextpid(sqlite3 *db, uint64_t *pid);
int obj_ispresent(sqlite3 *db, uint64_t pid, uint64_t oid);
int obj_pid_isempty(sqlite3 *db, uint64_t pid);
int obj_get_type(sqlite3 *db, uint64_t pid, uint64_t oid);
int obj_get_oids_in_pid(sqlite3 *db, uint64_t pid, uint64_t initial_oid,
			uint64_t alloc_len, uint8_t *outdata, 
			uint64_t *used_outlen, uint64_t *add_len, 
			uint64_t *cont_id);
int obj_get_cids_in_pid(sqlite3 *db, uint64_t pid, uint64_t initial_cid,
			uint64_t alloc_len, uint8_t *outdata, 
			uint64_t *used_outlen, uint64_t *add_len, 
			uint64_t *cont_id);
int obj_get_all_pids(sqlite3 *db, uint64_t initial_oid, uint64_t alloc_len,
		     uint8_t *outdata, uint64_t *used_outlen, 
		     uint64_t *add_len, uint64_t *cont_id);
#endif /* __OBJ_H */
