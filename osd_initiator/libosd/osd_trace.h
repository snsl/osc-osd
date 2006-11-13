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

/****************************************************************/
// Basic tracing facility
/****************************************************************/
/* To use this API properly, do: 
 *
 * 1) Initialize:
 *     osd_trace_init
 * 2) set the level, and add tag-level pairs:
 *     osd_trace_add_string_tag_full, osd_trace_set_level.
 * 3) osd_trace_init_done
 *     complete setting the trace-tags.
 *
 * The above set of operations is intended to be performed while
 * reading the command line arguments.
 *
 * None of the APIs in this module are thread-safe. 
 */

#ifndef OSD_TRACE_H
#define OSD_TRACE_H

#ifdef __KERNEL__
// kernel 
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/sched.h>

#define PRINT printk 

#else
// user-space
#include <stdio.h>
#define PRINT printf
#endif

#include <stdarg.h>

/* Enumerated list of tags.
 *
 * These tags are also internally represented in string format.  For
 * example, tag OSD_TRACE_ISD_LOGIN, is represented as "ISD_LOGIN".
 * This allows converting between a user-friendly string
 * representation and a compact integer representation.
 */
typedef enum {
    OSD_TRACE_ISD = 0,


    OSD_TRACE_SO_STACK,
    OSD_TRACE_SO_DBG1,      
    OSD_TRACE_SO_SENSE,       
    OSD_TRACE_SO_ALL,      
    
    // osd req trace levels
    OSD_TRACE_REQ,
    
    //t10 formatting traces
    OSD_TRACE_T10,

    //security library traces
    OSD_TRACE_SEC,

    //simulator traces
    OSD_TRACE_SIM,

    OSD_TRACE_OSD_ALL,    // Trace all of OSD

    OSD_TRACE_FE,
    OSD_TRACE_PL,
    OSD_TRACE_HNS,

    // utils trace levels
    OSD_TRACE_UTIL_DEBUG,

    // OC tags
    OSD_TRACE_OC_CRT,     // Co-routines
    OSD_TRACE_OC_UTL,     // Utilities
    OSD_TRACE_OC_IO,      // The IO client
    OSD_TRACE_OC_FS,      // The free-space 
    OSD_TRACE_OC_BT,      // The b-tree
    OSD_TRACE_OC_BPT,     // The b+-tree
    OSD_TRACE_OC_SN,      // The s-node
    OSD_TRACE_OC_CAT,     // The catalog
    OSD_TRACE_OC_OCT,     // object catalog
    OSD_TRACE_OC_PCT,     // partition catalog
    OSD_TRACE_OC_RM,      // The resource-manager
    OSD_TRACE_OC_SM,      // The state-machine
    OSD_TRACE_OC_HC,      // The handle cache (object and partition)
    OSD_TRACE_OC_RT,      // Running Table
    OSD_TRACE_OC_PM,      // Page-Manager
    OSD_TRACE_OC_JL,      // Journal  TODO remove later. Adam
    OSD_TRACE_OC_LJL,      // Logical Journal
    OSD_TRACE_OC_RC,      // The IO client
    OSD_TRACE_OC_ALL,     // Trace all OC
    
    OSD_TRACE_LAST_TAG // used for internal purposes, do -not- use externally
} Osd_trace_tag;

// Initialize tracing
void osd_trace_init(void);

/* Add a [tag, level] pair to the set of traced tags.
 * This is a low-level API, it is easier to use the
 *     [osd_trace_add_string_tag_full]
 * function. The nicer API is supported only for user-mode. 
 */
void osd_trace_add_tag_lvl(Osd_trace_tag tag, int level);

// Set the debug level. The default is 1. This can be done once at the most. 
void osd_trace_set_level(int level);

/* The stage of initialization is complete. From now on no more -add_tag-
 * calls will be performed. 
 */
void osd_trace_init_done(void);

/* Are the [tag] and [level] traced?
 *
 * return 1 if true, 0 if false.
 */
int osd_trace_is_set(Osd_trace_tag tag, int level);

// Actual tracing functions
void osd_trace(Osd_trace_tag id,
               int level,
               const char *format_p,
               ...);

void osd_trace_v(Osd_trace_tag tag,
                 int level,
                 const char *format_p,
                 va_list args);

#ifdef __KERNEL__
/* Kernel mode.
 *
 * Only the bare minimum is supported here. This trims the size of the
 * kernel-module.
 */
#define OSD_TRACE(tag, level, args...)  osd_trace(tag, level, args)
#define osd_trace_buf(tag,level,buf,buf_len) \
   osd_trace(tag,level,"buffer has %d bytes.\n",buf_len)
#else 

/* Only for user mode. 
 *
 * Here we define everything. 
 */

#define OSD_TRACE(tag, level, args...)                              \
do { char s[200]; snprintf(s,sizeof(s),args);                                  \
     osd_trace(tag, level, "<%s:%d> %s", __FUNCTION__, __LINE__,s); \
} while(0)

/* A version of [osd_trace] that writes to a file. 
 */
void osd_trace_f(Osd_trace_tag id,
                 int level,
                 FILE *stream_p,
                 const char *format_p,
                 ...);

//printing hex bytes values of a buffer.
void osd_trace_buf(Osd_trace_tag tag,
		   int level,
		   const void * buf_p,
		   int buf_len);

/* Add a tag, or a tag:level pair to the traces.
 *
 * There are two supported formats:
 *  <tag>  <tag>:<level>
 *
 * The first format is assumed to be for level=1.
 */
void osd_trace_add_string_tag_full(char *str_p);

/* Print the list of tags.
 * Used to describe to the user what tags are supported.
  */
void osd_trace_print_tag_list(void);

#endif

#define OSD_ERROR(args...)   \
 PRINT("%s:%d ERROR!!! ",__FUNCTION__, __LINE__); PRINT(args);


#endif
