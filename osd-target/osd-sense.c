
#include <sys/types.h>
#include <stdint.h>

#include "util/osd-sense.h"
#include "util/util.h"
#include "util/osd-defs.h"

/*
 * Descriptor format sense data.  See spc3 p 31.  Returns length of
 * buffer used so far.
 */
int sense_header_build(uint8_t *data, int len, uint8_t key, uint16_t code,
		       uint8_t additional_len)
{
	if (len < 8)
		return 0;
	data[0] = 0x72;  /* current, not deferred */
	data[1] = key;
	data[2] = ASC(code);
	data[3] = ASCQ(code);
	data[7] = additional_len;  /* additional length, beyond these 8 bytes */
	return 8;
}

/*
 * OSD object identification sense data descriptor.  Always need one
 * of these, then perhaps some others.  This goes just beyond the 8
 * byte sense header above.  The length of this is 32. osd2r01 Sec 4.14.2.1
 */
int sense_info_build(uint8_t *data, int len, uint32_t not_init_funcs,
		     uint32_t completed_funcs, uint64_t pid, uint64_t oid)
{
	if (len < 32)
		return 0;
	data[0] = 0x6;
	data[1] = 0x1e;
	set_htonl(&data[8], not_init_funcs);
	set_htonl(&data[12], completed_funcs);
	set_htonll(&data[16], pid);
	set_htonll(&data[24], oid);
	return 32;
}

/*
 * SPC-3 command-specific information sense data descriptor.  p. 33.
 */
int sense_csi_build(uint8_t *data, int len, uint64_t csi)
{
	if (len < 12)
		return 0;
	data[0] = 0x1;
	data[1] = 0xa;
	set_htonll(&data[4], csi);
	return 12;
}

/*
 * Helper to create sense data where no processing was initiated or completed,
 * and just a header and basic info descriptor are required.  Assumes full 252
 * byte sense buffer.
 */
int sense_basic_build(uint8_t *sense, uint8_t key, uint16_t code,
		      uint64_t pid, uint64_t oid)
{
	uint8_t off = 0;
	uint8_t len = OSD_MAX_SENSE;
	uint32_t nifunc = 0x303010b0;  /* non-reserved bits */

	off = sense_header_build(sense+off, len-off, key, code, 32);
	off += sense_info_build(sense+off, len-off, nifunc, 0, pid, oid);
	return off;
}

/*
 * Same as basic_build?  Delete one.
 */
int sense_build_sdd(uint8_t *sense, uint8_t key, uint16_t code,
		    uint64_t pid, uint64_t oid)
{
	uint8_t off = 0;
	uint8_t len = OSD_MAX_SENSE;
	uint32_t nifunc = 0x303010b0;  /* non-reserved bits */

	off = sense_header_build(sense+off, len-off, key, code, 32);
	off += sense_info_build(sense+off, len-off, nifunc, 0, pid, oid);
	return off;
}

/*
 * Sense header, info section with pid and oid, and "info" section
 * with an arbitrary u64.
 */
int sense_build_sdd_csi(uint8_t *sense, uint8_t key, uint16_t code,
		        uint64_t pid, uint64_t oid, uint64_t csi)
{
	uint8_t off = 0;
	uint8_t len = OSD_MAX_SENSE;
	uint32_t nifunc = 0x303010b0;  /* non-reserved bits */

	off = sense_header_build(sense+off, len-off, key, code, 44);
	off += sense_info_build(sense+off, len-off, nifunc, 0, pid, oid);
	off += sense_csi_build(sense+off, len-off, csi);
	return off;
}

