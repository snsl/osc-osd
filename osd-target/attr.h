#ifndef __ATTR_H
#define __ATTR_H

#include <sqlite3.h>
#include "osd-types.h"

int attr_initialize(struct db_context *dbc);

int attr_finalize(struct db_context *dbc);

const char *attr_getname(struct db_context *dbc);

int attr_set_attr(struct db_context *dbc, uint64_t pid, uint64_t oid, 
		  uint32_t page, uint32_t number, const void *val, 
		  uint16_t len);

int attr_delete_attr(struct db_context *dbc, uint64_t pid, uint64_t oid, 
		     uint32_t page, uint32_t number);

int attr_delete_all(struct db_context *dbc, uint64_t pid, uint64_t oid);

int attr_get_attr(struct db_context *dbc, uint64_t pid, uint64_t oid,
		  uint32_t page, uint32_t number, uint64_t outlen,
		  void *outdata, uint8_t listfmt, uint32_t *used_outlen);

int attr_get_val(struct db_context *dbc, uint64_t pid, uint64_t oid,
		 uint32_t page, uint32_t number, uint64_t outlen,
		 void *outdata, uint32_t *used_outlen);

int attr_get_page_as_list(struct db_context *dbc, uint64_t pid, uint64_t oid,
			  uint32_t page, uint64_t outlen, void *outdata,
			  uint8_t listfmt, uint32_t *used_outlen);

int attr_get_for_all_pages(struct db_context *dbc, uint64_t pid, uint64_t oid, 
			   uint32_t number, uint64_t outlen, void *outdata,
			   uint8_t listfmt, uint32_t *used_outlen);

int attr_get_all_attrs(struct db_context *dbc, uint64_t pid, uint64_t oid,
		       uint64_t outlen, void *outdata, uint8_t listfmt, 
		       uint32_t *used_outlen);

int attr_get_dir_page(struct db_context *dbc, uint64_t pid, uint64_t oid, 
		      uint32_t page, uint64_t outlen, void *outbuf,
		      uint8_t listfmt, uint32_t *used_outlen);

#endif /* __ATTR_H */
