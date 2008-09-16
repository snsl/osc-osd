#include <stdlib.h>
#include <stdio.h>
#include <sqlite3.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>

#include "osd-types.h"
#include "util/osd-defs.h"
#include "db.h"
#include "obj.h"
#include "attr.h"
#include "coll.h" 
#include "mtq.h"
#include "util/util.h"
#include "list-entry.h"

/*
 * mtq: mutitable query, all queries dealing with >=2 tables are implemented
 * here
 */

/*
 * return values:
 * -ENOMEM: out of memory
 * -EINVAL: invalid argument
 * 0: success
 */
int mtq_run_query(struct db_context *dbc, uint64_t pid, uint64_t cid, 
		  struct query_criteria *qc, void *outdata, 
		  uint32_t alloc_len, uint64_t *used_outlen)
{
	int ret = 0;
	int pos = 0;
	char *cp = NULL;
	const char *op = NULL;
	char select_stmt[MAXSQLEN];
	char *SQL = NULL;
	uint8_t *p = NULL;
	uint32_t i = 0;
	uint32_t sqlen = 0;
	uint32_t factor = 2; /* this query fills space quickly */
	uint64_t len = 0;
	sqlite3_stmt *stmt = NULL;
	sqlite3 *db = dbc->db;

	if (!db || !qc || !outdata || !used_outlen) {
		ret = -EINVAL;
		goto out;
	}

	if (qc->query_type == 0)
		op = " UNION ";
	else if (qc->query_type == 1)
		op = " INTERSECT ";
	else {
		ret = -EINVAL;
		goto out;
	}

	SQL = Malloc(MAXSQLEN*factor);
	if (SQL == NULL) {
		ret = -ENOMEM;
		goto out;
	}
	cp = SQL;
	sqlen = 0;

	/*
	 * XXX:SD the spec does not mention whether min or max values have to
	 * in tested with '<' or '<='. We assume the tests are inclusive of
	 * boundaries, i.e. use '<=' for comparison.
	 */

	/* build the SQL statment */
	sprintf(select_stmt, "SELECT attr.oid FROM %s AS oc, "
		" attr WHERE oc.pid = attr.pid AND oc.oid = attr.oid AND "
		" oc.pid = %llu AND oc.cid = %llu ", coll_getname(dbc),
		llu(pid), llu(cid));
	sprintf(cp, select_stmt);
	sqlen += strlen(cp);
	cp += sqlen;
	for (i = 0; i < qc->qc_cnt; i++) {
		sprintf(cp, " AND attr.page = %u AND attr.number = %u ",
			qc->page[i], qc->number[i]);
		if (qc->min_len[i] > 0)
			cp = strcat(cp, " AND ? <= attr.value ");
		if (qc->max_len[i] > 0)
			cp = strcat(cp, " AND attr.value <= ? ");

		if ((i+1) < qc->qc_cnt) {
			cp = strcat(cp, op);
			cp = strcat(cp, select_stmt);
		}
		sqlen += strlen(cp);

		if (sqlen >= (MAXSQLEN*factor - 400)) {
			factor *= 2;
			SQL = realloc(SQL, MAXSQLEN*factor);
			if (!SQL) {
				ret = -ENOMEM;
				goto out;
			}
		}
		cp = SQL + sqlen;
	}
	cp = strcat(cp, " GROUP BY attr.oid ORDER BY attr.oid;");

	ret = sqlite3_prepare(db, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: sqlite3_prepare", __func__);
		ret = -EIO;
		goto out;
	}

	/* bind the values */
	pos = 1;
	for (i = 0; i < qc->qc_cnt; i++) {
		if (qc->min_len[i] > 0) {
			ret = sqlite3_bind_blob(stmt, pos, qc->min_val[i],
						qc->min_len[i],
						SQLITE_TRANSIENT);
			if (ret != SQLITE_OK) {
				error_sql(db, "%s: bind min_val @ %d",
					  __func__, pos);
				goto out_finalize;
			}
			pos++;
		}
		if (qc->max_len[i] > 0) {
			ret = sqlite3_bind_blob(stmt, pos, qc->max_val[i],
						qc->max_len[i],
						SQLITE_TRANSIENT);
			if (ret != SQLITE_OK) {
				error_sql(db, "%s: bind max_val @ %d",
					  __func__, pos);
				goto out_finalize;
			}
			pos++;
		}
	}

	/* execute the query */
	p = outdata;
	p += ML_ODL_OFF; 
	len = ML_ODL_OFF - 8; /* subtract len of addition_len */
	*used_outlen = ML_ODL_OFF;
	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		if ((alloc_len - len) > 8) {
			/* 
			 * TODO: query is a multi-object command, so delete
			 * the objects from the collection, once they are
			 * selected
			 */
			set_htonll(p, sqlite3_column_int64(stmt, 0));
			*used_outlen += 8;
		}
		p += 8; 
		if (len + 8 < len)
			len = 0xFFFFFFFFFFFFFFFF; /* osd2r01 Sec 6.18.3 */
		else
			len += 8;
	}
	if (ret != SQLITE_DONE) {
		error_sql(db, "%s: sqlite3_step", __func__);
		ret = -EIO;
		goto out_finalize;
	}
	ret = 0;
	set_htonll(outdata, len);

out_finalize:
	ret = sqlite3_finalize(stmt); 
	if (ret != SQLITE_OK)
		error_sql(db, "%s: finalize", __func__);

out:
	free(SQL);
	return ret;
}


int mtq_list_oids_attr(sqlite3 *db, uint64_t pid, uint64_t initial_oid, 
		       struct getattr_list *get_attr, uint64_t alloc_len,
		       void *outdata, uint64_t *used_outlen, 
		       uint64_t *add_len, uint64_t *cont_id)
{
	int ret = 0;
	char *cp = NULL;
	char *SQL = NULL;
	uint64_t len = 0;
	uint64_t oid = 0;
	uint8_t *head = NULL, *tail = NULL;
	uint32_t i = 0;
	uint32_t factor = 2; /* this query uses space fast */
	uint32_t attr_list_len = 0; /*XXX:SD see below */
	uint32_t sqlen = 0;
	const void *val = NULL;
	sqlite3_stmt *stmt = NULL;

	if (!db || !get_attr || !outdata || !used_outlen || !add_len) {
		ret = -EINVAL;
		goto out;
	}

	if (get_attr->sz == 0) {
		ret = -EINVAL;
		goto out;
	}

	SQL = Malloc(MAXSQLEN*factor);
	if (!SQL) {
		ret = -ENOMEM;
		goto out;
	}

	/* 
	 * For each attribute requested, create a select statement,
	 * which will try to index into the attr table with a full key rather
	 * than just (pid, oid) prefix key. Analogous to loop unrolling.
	 */
	cp = SQL;
	sqlen = 0;
	sprintf(SQL, "SELECT obj.oid as myoid, attr.page, attr.number, "
		" attr.value FROM obj, attr "
		" WHERE obj.pid = attr.pid AND obj.oid = attr.oid AND "
		" obj.pid = %llu AND obj.type = %u AND "
		" attr.page = %u AND attr.number = %u AND obj.oid >= %llu "
		" UNION ALL ", llu(pid), USEROBJECT, USER_TMSTMP_PG, 0, 
		llu(initial_oid));
	sqlen += strlen(SQL);
	cp += sqlen;

	for (i = 0; i < get_attr->sz; i++) {
		sprintf(cp, "SELECT obj.oid as myoid, attr.page, attr.number, "
			" attr.value FROM obj, attr "
			" WHERE obj.pid = attr.pid AND obj.oid = attr.oid AND "
			" obj.pid = %llu AND obj.type = %u AND "
			" attr.page = %u AND attr.number = %u AND "
			" obj.oid >= %llu ",
			llu(pid), USEROBJECT, get_attr->le[i].page, 
			get_attr->le[i].number, llu(initial_oid));
		if (i < (get_attr->sz - 1))
			strcat(cp, " UNION ALL ");

		sqlen += strlen(cp);
		if (sqlen > (MAXSQLEN*factor - 400)) {
			factor *= 2;
			SQL = realloc(SQL, MAXSQLEN*factor);
			if (!SQL) {
				ret = -ENOMEM;
				goto out;
			}
		}

		cp = SQL + sqlen;
	}
	sprintf(cp, " ORDER BY myoid; "); 

	ret = sqlite3_prepare(db, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: sqlite3_prepare", __func__);
		goto out;
	}

	/* execute the statement */
	head = tail = outdata;
	attr_list_len = 0;
	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		/*
		 * XXX:SD The spec is inconsistent in applying padding and
		 * alignment rules. Here we make changes to the spec. In our
		 * case object descriptor format header (table 79) is 16B
		 * instead of 12B, and attributes list length field is 4B
		 * instead of 2B as defined in spec, and starts at byte 12
		 * in the header (not 10).  ODE is object descriptor entry.
		 */
		uint64_t oid = sqlite3_column_int64(stmt, 0);
		uint32_t page = sqlite3_column_int64(stmt, 1);
		uint32_t number = sqlite3_column_int64(stmt, 2);
		uint16_t len = sqlite3_column_bytes(stmt, 3);

		if (page == USER_TMSTMP_PG && number == 0) {
			/* look ahead in the buf to see if there is space */
			if (alloc_len >= 16) {
				/* start attr list of 'this' ODE */
				set_htonll(tail, oid);
				memset(tail + 8, 0, 4);  /* reserved */
				if (head != tail) {
					/* fill attr_list_len of prev ODE */
					set_htonl(head, attr_list_len);
					head = tail;
					attr_list_len = 0;
				}
				alloc_len -= 16;
				tail += 16;
				head += 12;  /* points to attr-list-len */
				*used_outlen += 16;
			} else {
				if (head != tail) {
					/* fill attr_list_len of prev ODE */
					set_htonl(head, attr_list_len);
					head = tail;
					attr_list_len = 0;
				}
				if (*cont_id == 0)
					*cont_id = oid;
			}
			*add_len += 16;
			continue;
		} 
		if (alloc_len >= 16) {
			/* osd_debug("%s: oid %llu, page %u, number %u, len %u",
				  __func__, llu(oid), page, number, len); */
			val = sqlite3_column_blob(stmt, 3);
			ret = le_pack_attr(tail, alloc_len, page, number, len, 
					   val);
			assert (ret != -EOVERFLOW);
			if (ret > 0) {
				alloc_len -= ret;
				tail += ret;
				attr_list_len += ret;
				*used_outlen += ret;
				if (alloc_len < 16){
					set_htonl(head, attr_list_len);
					head = tail;
					attr_list_len = 0;
					if (*cont_id == 0)
						*cont_id = oid;
				}
			} else {
				goto out_finalize;
			}
		} else {
			if (head != tail) {
				/* fill attr_list_len of this ODE */
				set_htonl(head, attr_list_len);
				head = tail;
				attr_list_len = 0;
				if (*cont_id == 0)
					*cont_id = oid;
			}
		}
		/* TODO: take care of overflow */
		*add_len += roundup8(4+4+2+len);
	}
	if (ret != SQLITE_DONE) {
		error_sql(db, "%s: query execution failed. SQL %s, add_len "
			  " %llu attr_list_len %u", __func__, SQL, 
			  llu(*add_len), attr_list_len);
		goto out_finalize;
	}
	if (head != tail) {
		set_htonl(head, attr_list_len);
		head += (4 + attr_list_len);
		assert(head == tail);
	}

	ret = 0;

out_finalize:
	if (sqlite3_finalize(stmt) != SQLITE_OK) {
		ret = -EIO;
		error_sql(db, "%s: finalize", __func__);
	}

out:
	free(SQL);
	return ret;
}

int mtq_set_member_attrs(struct db_context *dbc, uint64_t pid, uint64_t cid, 
			 struct setattr_list *set_attr)
{
	int ret = 0;
	int factor = 1;
	uint32_t i = 0;
	char *cp = NULL;
	char *SQL = NULL;
	size_t sqlen = 0;
	sqlite3_stmt *stmt = NULL;
	sqlite3 *db = dbc->db;

	if (db == NULL || set_attr == NULL) {
		ret = -EINVAL;
		goto out;
	}

	if (set_attr->sz == 0) {
		ret = 0;
		goto out;
	}

	SQL = Malloc(MAXSQLEN*factor);
	if (!SQL) {
		ret = -ENOMEM;
		goto out;
	}

	cp = SQL;
	sqlen = 0;
	sprintf(SQL, "INSERT OR REPLACE INTO attr ");
	sqlen += strlen(SQL);
	cp += sqlen;

	for (i = 0; i < set_attr->sz; i++) {
		sprintf(cp, " SELECT %llu, oid, %u, %u, ? FROM %s "
			" WHERE cid = %llu ", llu(pid), set_attr->le[i].page,
			set_attr->le[i].number, coll_getname(dbc), llu(cid));
		if (i < (set_attr->sz - 1))
			strcat(cp, " UNION ALL ");
		sqlen += strlen(cp);
		if (sqlen > (MAXSQLEN*factor - 100)) {
			factor *= 2;
			SQL = realloc(SQL, MAXSQLEN*factor);
			if (!SQL) {
				ret = -ENOMEM;
				goto out;
			}
		}
		cp = SQL + sqlen;
	}
	strcat(cp, " ;");

	ret = sqlite3_prepare(db, SQL, strlen(SQL)+1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		error_sql(db, "%s: sqlite3_prepare", __func__);
		goto out;
	}

	/* bind values */
	for (i = 0; i < set_attr->sz; i++) {
		ret = sqlite3_bind_blob(stmt, i+1, set_attr->le[i].cval, 
					set_attr->le[i].len,
					SQLITE_TRANSIENT);
		if (ret != SQLITE_OK) {
			error_sql(db, "%s: blob @ %u", __func__, i+1);
			goto out_finalize;
		}
	}

	/* execute the statement */
	while ((ret = sqlite3_step(stmt)) == SQLITE_BUSY);
	if (ret != SQLITE_DONE) {
		error_sql(db, "%s: sqlite3_step", __func__);
		ret = -EIO;
		goto out_finalize;
	}
	ret = 0;

out_finalize:
	if (sqlite3_finalize(stmt) != SQLITE_OK)
		error_sql(db, "%s: finalize", __func__);

out:
	free(SQL);
	return ret;
}


