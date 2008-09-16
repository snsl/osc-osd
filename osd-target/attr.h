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
#endif /* __ATTR_H */
