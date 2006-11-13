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

#ifndef OSD_UTIL_H
#define OSD_UTIL_H

#include "osd_trace.h"

#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/sched.h>

#else /* __KERNEL__ */
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#endif /* __KERNEL__ */
/*
 * Byte Order
 */

#ifdef GPFS_OSD
#include <endian.h>
#else
#include <asm/byteorder.h>
#endif

#ifdef __KERNEL__
#ifndef __BYTE_ORDER
#ifdef __BIG_ENDIAN
#define __BYTE_ORDER __BIG_ENDIAN
#endif
#ifdef __LITTLE_ENDIAN
#define __BYTE_ORDER __LITTLE_ENDIAN
#endif
#endif
#endif

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

/*assert*/
#ifdef __KERNEL__
#define osd_assert(cond) \
do { if (!(cond)) { \
       OSD_ERROR("assertion failed (%s)\n",#cond); \
   } } while(0)

#else
#include <assert.h>

#define osd_assert(cond) assert(cond)
#if LODESTONE_DEBUG
#define osd_debugassert(cond) assert(cond)
#else
#define osd_debugassert(cond)
#endif
#endif /*ifdef kernel*/

#define osd_malloc(size) malloc(size)
#define osd_free(p) free(p)

typedef enum {
    OSD_TARGET_HNS = 1,      // A harness configuration
    OSD_TARGET_SIM = 2,      // A simulator harness configuration    
    OSD_TARGET_NORMAL = 3,   // A regular configuration
} Osd_target_init_rc;

// The type of the tester function
typedef void* (*Osd_tester_fun)(void*);

#endif /* OSD_UTIL_H */
