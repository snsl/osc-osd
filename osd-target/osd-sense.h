#ifndef __OSD_SENSE_H
#define __OSD_SENSE_H

/* including 8 byte header, and additional len = 244, as given by spc3 */
#define MAX_SENSE_LEN 252

/* stage of command. osd2 rev 10 table 33, 34, 35 */
/* setters */
#define SET_OSD_VALIDATON_STG(s) (s = (s) | (0x1U << 7))
#define SET_OSD_CMD_CAP_V_STG(s) (s = (s) | (0x1U << 5))
#define SET_OSD_COMMAND_STG(s) (s = (s) | (0x1U << 4))
#define SET_OSD_IMP_ST_ATT_STG (s = (s) | (0x1U << 12))
#define SET_OSD_SA_CAP_V_STG (s = (s) | (0x1U << 21))
#define SET_OSD_SET_ATT_STG (s = (s) | (0x1U << 20))
#define SET_OSD_GA_CAP_V_STG (s = (s) | (0x1U << 29))
#define SET_OSD_GET_ATT_STG (s = (s) | (0x1U << 28))

/* unsetters */
#define UNSET_OSD_VALIDATON_STG(s) (s = (s) & ~(0x1U << 7))
#define UNSET_OSD_CMD_CAP_V_STG(s) (s = (s) | ~(0x1U << 5))
#define UNSET_OSD_COMMAND_STG(s) (s = (s) | ~(0x1U << 4))
#define UNSET_OSD_IMP_ST_ATT_STG (s = (s) | ~(0x1U << 12))
#define UNSET_OSD_SA_CAP_V_STG (s = (s) | ~(0x1U << 21))
#define UNSET_OSD_SET_ATT_STG (s = (s) | ~(0x1U << 20))
#define UNSET_OSD_GA_CAP_V_STG (s = (s) | ~(0x1U << 29))
#define UNSET_OSD_GET_ATT_STG (s = (s) | ~(0x1U << 28))

/* scsi sense keys (SSK) defined in table 27, SPC3 r23 */
#define OSD_SSK_NO_SENSE (0x00)
#define OSD_SSK_RECOVERED_ERROR (0x01)
#define OSD_SSK_NOT_READY (0x02)
#define OSD_SSK_MEDIUM_ERROR (0x03)
#define OSD_SSK_HARDWARE_ERROR (0x04)
#define OSD_SSK_ILLEGAL_REQUEST (0x05)
#define OSD_SSK_UNIT_ATTENTION (0x06)
#define OSD_SSK_DATA_PROTECTION (0x07)
#define OSD_SSK_BLANK_CHECK (0x08)
#define OSD_SSK_VENDOR_SPECIFIC (0x09)
#define OSD_SSK_COPY_ABORTED (0x0A)
#define OSD_SSK_ABORTED_COMMAND (0x0B)
#define OSD_SSK_OBSOLETE_SENSE_KEY (0x0C)
#define OSD_SSK_VOLUME_OVERFLOW (0x0D)
#define OSD_SSK_MISCOMPARE (0x0E)
#define OSD_SSK_RESERVED_SENSE_KEY (0x0F)

/* OSD specific additional sense codes (ASC), defined in table 28 SPC3 r23.
 * OSD specific ASC from osd2 T10 r10 section 4, 5, 6, 7 */
#define OSD_ASC_INVALID_COMMAND_OPERATION_CODE (0x2000)
#define OSD_ASC_INVALID_DATA_OUT_BUFFER_INTEGRITY_CHECK_VALUE (0x260F)
#define OSD_ASC_INVALID_FIELD_IN_CDB (0x2400)
#define OSD_ASC_INVALID_FIELD_IN_PARAMETER_LIST (0x2600)
#define OSD_ASC_LOGICAL_UNIT_NOT_READY_FORMAT_IN_PROGRESS (0x0404)
#define OSD_ASC_NONCE_NOT_UNIQUE (0x2406)
#define OSD_ASC_NONCE_TIMESTAMP_OUT_OF_RANGE (0x2407)
#define OSD_ASC_PARAMETER_LIST_LENGTH_ERROR (0x1A00)
#define OSD_ASC_PARTITION_OR_COLLECTION_CONTAINS_USER_OBJECTS (0x2C0A)
#define OSD_ASC_READ_PAST_END_OF_USER_OBJECT (0x3B17)
#define OSD_ASC_RESERVATIONS_RELEASED (0x2A04)
#define OSD_ASC_QUOTA_ERROR (0x5507)
#define OSD_ASC_SECURITY_AUDIT_VALUE_FROZEN (0x2405)
#define OSD_ASC_SECURITY_WORKING_KEY_FROZEN (0x2406)
#define OSD_ASC_SYSTEM_RESOURCE_FAILURE (0x5500)

#define ASC(x) ((x & 0xFF00) >> 8)
#define ASCQ(x) (x & 0x00FF)

/* including 8 byte header, and additional len = 244, as given by spc3 */
#define MAX_SENSE_LEN 252

/* Descriptor format sense data (fsd). SPC3 r23, 4.5.2.1 */
typedef struct scsi_desc_fsd {
	uint8_t resp_code; /* 0x72 */
	uint8_t sense_key;
	uint8_t asc;
	uint8_t ascq;
	uint8_t reserved[3];
	uint8_t add_sense_len;
} scsi_desc_fsd_t;

/*
 * These are sense descriptors.  One or more of them get tacked behind
 * a header that is assembled in stgt's descriptor_sense_data_build().
 */

/* 
 * OSD error identification sense data descriptor (sdd). osd2 T10 r10, 
 * Sec 4.14.2.1 
 */
typedef struct osd_err_id_sdd {
    uint8_t key; /* 0x6 */
    uint8_t add_len; /* 0x1E */
    uint8_t reserved[6];
    uint32_t not_initiated_cmd_funcs;
    uint32_t completed_cmd_funcs;
    uint64_t pid;
    uint64_t oid;
} __attribute__((packed)) osd_err_id_sdd_t;

/* 
 * OSD attribute identification sense data descriptor (sdd). osd2 T10 r10 
 * 4.14.2.3 
 */
typedef struct osd_attr_id_sdd_t {
    uint8_t key; /* 0x8 */
    uint8_t add_len; /* num attributes*8 - 2 */
    uint8_t reserved[2];
    uint32_t page;
    uint32_t number;
} __attribute__((packed)) osd_attr_id_sdd_t;

int sense_header_build(uint8_t *data, int len, uint8_t key, uint8_t asc,
					   uint8_t ascq, uint8_t additional_len);
int sense_info_build(uint8_t *data, int len, uint32_t not_init_funcs,
					 uint32_t completed_funcs, uint64_t pid, uint64_t oid);
int sense_basic_build(uint8_t *sense, uint8_t key, uint8_t asc, uint8_t ascq,
					  uint64_t pid, uint64_t oid);
int sense_build_sdd(uint8_t *sense, uint8_t key, uint16_t sense_code,
					uint64_t pid, uint64_t oid);

#endif /* __OSD_SENSE_H */
