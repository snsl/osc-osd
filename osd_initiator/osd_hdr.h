#ifndef __OSD_HDR_H
#define __OSD_HDR_H

#include "osd_cmds.h"

/* Valid OSD command list supported by Object Storage Devices; these
 * are encoded in the service action field of the command */
const unsigned long valid_osd_cmd_list[] = { 

	/* Supported by simulation targets */
	OSD_CREATE, 
	OSD_CREATE_PARTITION,
	OSD_FORMAT_OSD,
	OSD_GET_ATTRIBUTES,
	OSD_LIST,
	OSD_READ,
	OSD_REMOVE, 
	OSD_REMOVE_PARTITION, 
	OSD_SET_ATTRIBUTES, 
	OSD_SET_KEY,
	OSD_WRITE,

	/* Unsupported by simulation targets */
	OSD_APPEND,
	OSD_CREATE_AND_WRITE,
	OSD_CREATE_COLLECTION,
	OSD_FLUSH,
	OSD_FLUSH_COLLECTION,
	OSD_FLUSH_OSD,
	OSD_FLUSH_PARTITION,
	OSD_LIST_COLLECTION,
	OSD_REMOVE_COLLECTION,
	OSD_SET_MASTER_KEY,
};
#endif /* __OSD_HDR_H */
