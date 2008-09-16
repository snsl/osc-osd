#ifndef __CDB_H
#define __CDB_H

#define CDB_OPCODE (0x7F)
#define ADD_CDB_LEN (192)

int osdemu_cmd_submit(struct osd_device *osd, uint8_t *cdb,
                      const uint8_t *data_in, uint64_t data_in_len,
		      uint8_t **data_out, uint64_t *data_out_len,
		      uint8_t *sense_out, int *senselen_out);

#endif /* __CDB_H */
