/*************************************************************
** Copyright 2004, 2005. IBM Corp.** All Rights Reserved.***
**************************************************************
** Redistribution and use in source and binary forms, with or without
** modifications, are permitted provided that the following conditions 
** are met:
** 
** i.  Redistribution of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**
** ii. Redistribution in binary form must reproduce the above copyright 
**     notice, this list of conditions and the following disclaimer in the 
**     documentation and/or other materials provided with the distribution.
**
** iii.Neither the name(s) of IBM Corp.** nor the names of its/their
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
*******************************************************************
** THIS SOFTWARE IS PROVIDED BY IBM CORP. AND CONTRIBUTORS "AS IS" 
** AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING , BUT NOT LIMITED
** TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A 
** PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE IBM
** CORP. OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
** (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
** OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
** STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
** ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED 
** OF THE POSSIBILITY OF SUCH DAMAGE.
** 
*****************************************************************************
*****************************************************************************/
#ifndef OSD_OPS_UTILS_H
#define OSD_OPS_UTILS_H

#ifdef __KERNEL__
#define PRINT printk
#define OSD_OPS_GETPID current->pid

static inline void * os_malloc(int size)
{
    return kmalloc(size, GFP_KERNEL);
}
static inline void os_free(void * ptr)
{
    kfree(ptr);
}

#else
#include <stdlib.h>
#include <stdio.h>
#define PRINT printf
#define OSD_OPS_GETPID getpid()


static inline void * os_malloc(int size)
{
    return malloc(size);
}

static inline void os_free(void * ptr)
{
    free(ptr);
}

#endif

#define osd_assert(cond) \
do { if (!(cond)) { \
       OSD_OPS_ERROR("assertion failed (%s)\n",#cond); \
   } } while(0)

#ifndef __KERNEL__

#include <byteswap.h>

#if __BYTE_ORDER == __BIG_ENDIAN
BIG??
#define NTOHLL(x) (x)
#define HTONLL(x) (x)
#define NTOHL(x)  (x)
#define HTONL(x)  (x)
#define NTOHS(x)  (x)
#define HTONS(x)  (x)
#else
#define NTOHLL(x) bswap_64(x)
#define HTONLL(x) bswap_64(x)
#define NTOHL(x)  bswap_32(x)
#define HTONL(x)  bswap_32(x)
#define NTOHS(x)  bswap_16(x)
#define HTONS(x)  bswap_16(x)
#endif

#else 
#define NTOHLL(x) swab64(x)
#define HTONLL(x) swab64(x)
#define NTOHL(x)  swab32(x)
#define HTONL(x)  swab32(x)
#define NTOHS(x)  swab16(x)
#define HTONS(x)  swab16(x)
#endif

#define TRACE_OPS_UTIL_DEBUG     0x00001000

#define OSD_PRINT_PID PRINT("pid %i:",OSD_OPS_GETPID)
#define OSD_PRINT_FILE PRINT("%s:",__FILE__)

#define OSD_OPS_ERROR PRINT

#endif //OSD_OPS_UTILS_H

