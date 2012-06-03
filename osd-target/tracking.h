/*
 * OSD Command Tracking
 *
 * Copyright (C) 2011 University of Connecticut. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdint.h>
#include "osd-util/osd-defs.h"
#include "osd-types.h"

struct ctp {
	uint64_t pid;
	uint64_t cid;
	uint16_t service_action;
	uint16_t status;
	uint8_t  percent_complete;
	uint8_t  senselen;
	uint8_t	 sense[OSD_MAX_SENSE];
	uint64_t number_of_members;
	uint64_t objects_processed;
	uint64_t newer_objects_skipped;
	uint64_t missing_objects_skipped;
};

/* init_ctp creates a command tracking page for the specified collection */

struct ctp *init_ctp(uint64_t pid, uint64_t cid, uint16_t service_action);

/* finish_ctp is called when the command completes.  The caller should
   set ctp->status and ctp->sense before calling finish_ctp.  The ctp
   struct will no longer be valid after this call */

void finish_ctp(struct osd_device *osd, struct ctp *ctp);

/* find_ctp returns a command tracking page in ctp struct format */
struct ctp *find_ctp(uint64_t pid, uint64_t cid);

/* get_ctp returns a command tracking page in attribute format */
int get_ctp(struct osd_device *osd, uint64_t pid, uint64_t oid,
	    uint32_t number, void *outbuf, uint64_t outlen,
	    uint8_t listfmt, uint32_t *used_outlen);


