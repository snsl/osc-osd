/* #include <stdlib.h> */
#include <stdint.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

#include "osd-defs.h"
#include "osd.h"
#include "list-entry.h"
#include "util/util.h"

/* 
 * retrieve attr in list entry format; tab 129 Sec 7.1.3.3
 *
 * returns: 
 * -EINVAL: error, alignment error
 * -EOVERFLOW: error, if not enough room to even start the entry
 * >0: success. returns number of bytes copied into buf.
 */
int le_pack_attr(void *buf, uint32_t buflen, uint32_t page, uint32_t number,
		 uint16_t valen, void *val)
{
	uint8_t pad = 0;
	uint8_t *cp = buf;
	uint32_t len = buflen;

	if (buflen < LE_VAL_OFF)
		return -EOVERFLOW;

	/* XXX: osd-errata: buf and buflen must be 8B aligned */
	if((buflen & 0x7) || ((uint64_t)buf & 0x7)) 
		return -EINVAL; 

	set_htonl_le(&cp[LE_PAGE_OFF], page);
	set_htonl_le(&cp[LE_NUMBER_OFF], number);

	/* length field is not modified to reflect truncation, sec 5.2.2.2 */
	set_htons_le(&cp[LE_LEN_OFF], valen);

	if (val != NULL) {
		buflen -= LE_VAL_OFF;
		if ((uint32_t)valen > buflen)
			valen = buflen;
		memcpy(&cp[LE_VAL_OFF], val, valen);
	} else {
		assert(valen == NULL_ATTR_LEN);
		valen = 0;
	}

	len = valen + LE_VAL_OFF;
	pad = (0x8 - (len & 0x7)) & 0x7;
	cp += len; 
	len += pad;
	while (pad--)
		*cp = 0, cp++;
	return len;
}

/*
 * retrieve attr in list entry format; tab 130 Sec 7.1.3.4
 *
 * returns:
 * -EINVAL: error, alignment error
 * -EOVERFLOW: error, if not enough room to even start the entry
 * >0: success. returns number of bytes copied into buf.
 */
int le_multiobj_pack_attr(void *buf, uint32_t buflen, uint64_t oid, 
			  uint32_t page, uint32_t number, uint16_t valen,
			  void *val)
{
	int ret = 0;
	uint8_t *cp = buf;

	if (buflen < MLE_VAL_OFF)
		return -EOVERFLOW;

	set_htonll_le(cp, oid);

	/* 
	 * test if layout of struct multiobj_list_entry is similar to 
	 * struct list_entry prefixed with oid. If it is then, we can call
	 * attr_pack_attr to pack remaining values.
	 */
	assert(MLE_VAL_OFF == (LE_VAL_OFF+sizeof(oid)));

	ret = le_pack_attr(cp+sizeof(oid), buflen-sizeof(oid), page, number,
			   valen, val);
	if (ret > 0)
		ret = MLE_PAGE_OFF + ret; /* add sizeof(oid) to length */
	return ret;
}

