#ifndef __MTQ_H
#define __MTQ_H

#include <sqlite3.h>
#include "osd-types.h"

int mtq_run_query(struct db_context *dbc, uint64_t pid, uint64_t cid, 
		  struct query_criteria *qc, void *outdata, 
		  uint32_t alloc_len, uint64_t *used_outlen);
int mtq_list_oids_attr(sqlite3 *db,  uint64_t pid, uint64_t initial_oid, 
		       struct getattr_list *get_attr, uint64_t alloc_len,
		       void *outdata, uint64_t *used_outlen, 
		       uint64_t *add_len, uint64_t *cont_id);
int mtq_set_member_attrs(struct db_context *dbc, uint64_t pid, uint64_t cid, 
			 struct setattr_list *set_attr);
#endif /* __MTQ_H */
