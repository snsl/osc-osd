

struct object {
	uint64_t pid;
	uint64_t oid;
};

struct object *obj_lookup(struct osd *osd, uint64_t pid, uint64_t oid);
int obj_insert(struct osd *osd, uint64_t pid, uint64_t oid);
void obj_free(struct object *obj);
