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

/* Writing command tracking info to the attribute database may be
   expensive.  So, just keep an array of command tracking pages in
   memory in a linked list.  When the command is complete, we will
   flush the attributes to the database.  The problem is that if the
   OSD target crashes before we flush, we can't return a meaningful
   SAM-4 error to initiator.  All we can do is return the previous
   command call or that "no command is using the collection."  */

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>

#include "tracking.h"
#include "attr.h"
#include "obj.h"
#include "list-entry.h"
#include "osd-util/osd-util.h"

#define NUM_CTPS 10
struct ctp_node {
	struct ctp ctp;
	struct ctp_node *next;
};

struct ctp_node *ctp_head = NULL, *ctp_tail = NULL;
int num_ctps = 0;

struct ctp *init_ctp(uint64_t pid, uint64_t cid, uint16_t service_action)
{
	struct ctp_node *ctpn = NULL;
	struct ctp *ctp;

	if (num_ctps == NUM_CTPS) {
		/* find a ctp that has completed.  if it has
		   completed, it has been flushed to the database, we
		   can remove from queue and reuse it */
		struct ctp_node **ctpnp;
		for (ctpnp = &ctp_head; *ctpnp; ctpnp = &((*ctpnp)->next)) {
			ctpn = *ctpnp;
			if (ctpn->ctp.status != 0xFFFF) {
				*ctpnp = ctpn->next;
				break;
			}
		}
		if (ctpn == NULL) {
			return NULL;
		}
	} else {
		ctpn = (struct ctp_node *)malloc(sizeof(*ctpn));
		if (num_ctps == 0) {
			ctp_head = ctp_tail = ctpn;
		}
		num_ctps++;
	}

	if (ctp_tail)
	    ctp_tail->next = ctpn;
	ctpn->next = NULL;
	ctp_tail = ctpn;

	ctp = &ctpn->ctp;

	ctp->pid = pid;
	ctp->cid = cid;
	ctp->service_action = service_action;
	ctp->status = 0xFFFF;
	ctp->percent_complete = 0;

	return ctp;
}

/* finish_ctp is called when the command completes.  The caller should
   set ctp->status and ctp->sense before calling finish_ctp.  The ctp
   struct will no longer be valid after this call */

void finish_ctp(struct osd_device *osd, struct ctp *ctp)
{
	attr_set_attr(osd->dbc, ctp->pid, ctp->cid, COLL_TRACKING_PG,
		      CTP_PERCENT_COMPLETE, &ctp->percent_complete,
		      sizeof(ctp->percent_complete));
	attr_set_attr(osd->dbc, ctp->pid, ctp->cid, COLL_TRACKING_PG,
		      CTP_ENDED_COMMAND_STATUS, &ctp->status,
		      sizeof(ctp->status));
	if (ctp->status) {
		attr_set_attr(osd->dbc, ctp->pid, ctp->cid, COLL_TRACKING_PG,
			      CTP_SENSE_DATA, ctp->sense, ctp->senselen);
	}
	if (ctp->number_of_members) {
		attr_set_attr(osd->dbc, ctp->pid, ctp->cid, COLL_TRACKING_PG,
			      CTP_NUMBER_OF_MEMBERS, &ctp->number_of_members,
			      sizeof(ctp->number_of_members));
	}
	if (ctp->objects_processed) {
		attr_set_attr(osd->dbc, ctp->pid, ctp->cid, COLL_TRACKING_PG,
			      CTP_OBJECTS_PROCESSED, &ctp->objects_processed,
			      sizeof(ctp->objects_processed));
	}
	if (ctp->newer_objects_skipped) {
		attr_set_attr(osd->dbc, ctp->pid, ctp->cid, COLL_TRACKING_PG,
			      CTP_NEWER_OBJECTS_SKIPPED,
			      &ctp->newer_objects_skipped,
			      sizeof(ctp->newer_objects_skipped));
	}
	if (ctp->missing_objects_skipped) {
		attr_set_attr(osd->dbc, ctp->pid, ctp->cid, COLL_TRACKING_PG,
			      CTP_MISSING_OBJECTS_SKIPPED,
			      &ctp->missing_objects_skipped,
			      sizeof(ctp->missing_objects_skipped));
	}
	return;
}

struct ctp *find_ctp(uint64_t pid, uint64_t cid)
{
	struct ctp_node *ctpn;
	for (ctpn = ctp_head; ctpn; ctpn = ctpn->next) {
		if (ctpn->ctp.pid == pid && ctpn->ctp.cid == cid) {
			return &ctpn->ctp;
		}
	}
	return NULL;
}

int get_ctp(struct osd_device *osd, uint64_t pid, uint64_t cid,
	    uint32_t number, void *outbuf, uint64_t outlen,
	    uint8_t listfmt, uint32_t *used_outlen)
{
	int ret = 0;
	const void *val = NULL;
	uint16_t len = 0;
	char name[ATTR_PAGE_ID_LEN];
	char path[MAXNAMELEN];
	uint8_t ll[8];
	uint64_t pcount;
	uint32_t page = COLL_TRACKING_PG;

	struct ctp* ctp = find_ctp(pid, cid);

	switch (number) {
	case 0:
		/*{COLL_PG + 4, 0, "INCITS T10 Command Tracking"},*/
		len = ATTR_PAGE_ID_LEN;
		sprintf(name, "INCITS T10 Command Tracking");
		val = name;
		break;
	case CTP_PERCENT_COMPLETE:
		if (ctp) {
			len = 1;
			val = &ctp->percent_complete;
		}
		break;

	case CTP_ACTIVE_COMMAND_STATUS:
		if (ctp && ctp->status == 0xFFFF) {
			set_htons(ll, ctp->service_action);
		} else {
			set_htons(ll, 0);
		}
		len = 2;
		val = ll;
		break;

	case CTP_ENDED_COMMAND_STATUS:
		if (ctp) {
			set_htons(ll, ctp->status);
			len = 2;
			val = ll;
		}
		break;

	case CTP_SENSE_DATA:
		if (ctp) {
			len = ctp->senselen;
			val = ctp->sense;
		}
		break;

	case CTP_NUMBER_OF_MEMBERS:
		if (ctp) {
			set_htonll(ll, ctp->number_of_members);
			len = 8;
			val = ll;
		}
		break;

	case CTP_OBJECTS_PROCESSED:
		if (ctp) {
			set_htonll(ll, ctp->objects_processed);
			len = 8;
			val = ll;
		}
		break;

	case CTP_NEWER_OBJECTS_SKIPPED:
		if (ctp) {
			set_htonll(ll, ctp->newer_objects_skipped);
			len = 8;
			val = ll;
		}
		break;

	case CTP_MISSING_OBJECTS_SKIPPED:
		if (ctp) {
			set_htonll(ll, ctp->missing_objects_skipped);
			len = 8;
			val = ll;
		}
		break;

	default:
		return OSD_ERROR;
	}

	if (val==NULL) {
		return attr_get_attr(osd->dbc, pid, cid, page, number,
				     outlen, outbuf, listfmt, used_outlen);
	}
	
	if (listfmt == RTRVD_SET_ATTR_LIST)
		ret = le_pack_attr(outbuf, outlen, page, number, len, val);
	else if (listfmt == RTRVD_MULTIOBJ_LIST)
		ret = le_multiobj_pack_attr(outbuf, outlen, cid, page, number,
					    len, val);
	else
		return OSD_ERROR;

	assert(ret == -EINVAL || ret == -EOVERFLOW || ret > 0);
	if (ret == -EOVERFLOW)
		*used_outlen = 0;
	else if (ret > 0)
		*used_outlen = ret;
	else
		return ret;

	return OSD_OK;
}

