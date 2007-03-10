#ifndef _SENSE_H
#define _SENSE_H

void osd_sense_extract(const uint8_t *sense, int len, int *key, int *asc_ascq);
char *osd_sense_as_string(const uint8_t *sense, int len);

#endif /* _SENSE_H */
