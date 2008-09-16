uint16_t get_list_item(uint8_t list, int num);

int get_attributes(struct osd_device *osd, uint8_t *cdb,
                   uint8_t *data_in, uint64_t data_in_len,
		   uint8_t **data_out, uint64_t *data_out_len,
		   uint8_t **sense_out, uint8_t *senselen_out, uint8_t cdbfmt);

int set_attributes(struct osd_device *osd, uint8_t *cdb,
                   uint8_t *data_in, uint64_t data_in_len,
		   uint8_t **data_out, uint64_t *data_out_len,
		   uint8_t **sense_out, uint8_t *senselen_out);


int osdemu_cmd_submit(struct osd_device *osd, uint8_t *cdb,
                      uint8_t *data_in, uint64_t data_in_len,
		      uint8_t **data_out, uint64_t *data_out_len,
		      uint8_t **sense_out, uint8_t *senselen_out);



