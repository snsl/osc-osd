#ifndef __OBJ_H
#define __OBJ_H

#include "osd-types.h"

int obj_insert(osd_t *osd, uint64_t pid, uint64_t oid);
int obj_delete(osd_t *osd, uint64_t pid, uint64_t oid);

#endif /* __OBJ_H */
