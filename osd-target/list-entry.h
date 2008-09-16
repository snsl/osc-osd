#ifndef __LIST_ENTRY_H
#define __LIST_ENTRY_H

#include <stdint.h>

int le_pack_attr(void *buf, uint32_t buflen, uint32_t page, uint32_t number,
		 uint16_t valen, const void *val);

int le_multiobj_pack_attr(void *buf, uint32_t buflen, uint64_t oid, 
			  uint32_t page, uint32_t number, uint16_t valen,
			  const void *val);

#endif /* __LIST_ENTRY_H */
