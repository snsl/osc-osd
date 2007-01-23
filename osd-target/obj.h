#ifndef __OBJ_H
#define __OBJ_H

#include "osd-types.h"

int obj_insert(struct osd_device *osd, uint64_t pid, uint64_t oid);
int obj_delete(struct osd_device *osd, uint64_t pid, uint64_t oid);

#endif /* __OBJ_H */
