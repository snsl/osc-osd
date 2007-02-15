#ifndef _USER_INTERFACE_SGIO_H
#define _USER_INTERFACE_SGIO_H

static const uint64_t pid = 0x10000LLU;
static const uint64_t oid = 0x10003LLU;

int inquiry_sgio(int fd);
int create_osd_sgio(int fd);
int write_osd_sgio(int fd);
int read_osd_sgio(int fd);

#endif
