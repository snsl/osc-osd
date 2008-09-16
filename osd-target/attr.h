#ifndef __ATTR_H
#define __ATTR_H

#include <sqlite3.h>
#include "osd-types.h"

int attr_set_attr(sqlite3 *db, uint64_t pid, uint64_t oid, uint32_t page, 
		  uint32_t number, const void *val, uint16_t len);
int attr_delete_attr(sqlite3 *db, uint64_t pid, uint64_t oid, uint32_t page, 
		     uint32_t number);
int attr_delete_all(sqlite3 *db, uint64_t pid, uint64_t oid);
int attr_set_attr(sqlite3 *db, uint64_t pid, uint64_t oid, uint32_t page,
		  uint32_t number, const void *val, uint16_t len);
int attr_get_attr(sqlite3 *db, uint64_t pid, uint64_t oid, uint32_t page, 
		  uint32_t number, void *outbuf, uint64_t outlen, 
		  uint8_t listfmt, uint32_t *used_outlen);
int attr_get_page_as_list(sqlite3 *db, uint64_t pid, uint64_t  oid,
			  uint32_t page, void *outbuf, uint64_t outlen,
			  uint8_t listfmt, uint32_t *used_outlen);
int attr_get_all_attrs(sqlite3 *db, uint64_t pid, uint64_t  oid, void *outbuf,
		       uint64_t outlen, uint8_t listfmt, 
		       uint32_t *used_outlen);
int attr_get_for_all_pages(sqlite3 *db, uint64_t pid, uint64_t  oid, 
			   uint32_t number, void *outbuf, uint64_t outlen,
			   uint8_t listfmt, uint32_t *used_outlen);
int attr_get_dir_page(sqlite3 *db, uint64_t pid, uint64_t oid, uint32_t page,
		      void *outbuf, uint64_t outlen, uint8_t listfmt,
		      uint32_t *used_outlen);
int attr_run_query_1(sqlite3 *db, uint64_t cid, struct query_criteria *qc, 
		     void *outdata, uint32_t alloc_len, uint64_t *used_outlen);
int attr_run_query_2(sqlite3 *db, uint64_t cid, struct query_criteria *qc, 
		     void *outdata, uint32_t alloc_len, uint64_t *used_outlen);
int attr_list_oids_attr(sqlite3 *db,  uint64_t pid, uint64_t initial_oid, 
			struct getattr_list *get_attr, uint64_t alloc_len,
			void *outdata, uint64_t *used_outlen, 
			uint64_t *add_len, uint64_t *cont_id);
int attr_set_member_attrs(sqlite3 *db, uint64_t pid, uint64_t cid, 
			  struct setattr_list *set_attr);
int attr_get_attr_value(sqlite3 *db, uint64_t pid, uint64_t oid, 
			uint32_t page, uint32_t number, void *outdata, 
			uint16_t len);
#endif /* __ATTR_H */
