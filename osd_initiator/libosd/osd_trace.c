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
#ifndef __KERNEL__
#include <assert.h>
#include <memory.h>
#include <stdlib.h>
#endif

#include "osd_trace.h"

#define TAG_MAX_SIZE (12)
#define TAG_MIN_SIZE (2)
#define LEVEL_MAX (4)
#define LEVEL_MIN (1)

#define MAX(x,y) ((x) > (y) ? (x) : (y))
    
// An array with a flag per tag. The flag is true if the tag should be traced. 
static unsigned char trace_tag_a[OSD_TRACE_LAST_TAG];

/* An array that matches a tag with its string representation. This
 *  array is set up upon initialization and allows quickly matching
 *  between a user's command line trace requests and numeric ids of
 *  tags.
 */
static char trace_tag_str_a[OSD_TRACE_LAST_TAG][TAG_MAX_SIZE];
static char trace_tag_str_a_prn[OSD_TRACE_LAST_TAG][TAG_MAX_SIZE];

// The trace level, we will probably support only two levels. 
static int trace_level;
static int level_has_been_set;

static char *convert_tag_to_string(Osd_trace_tag tag);

/****************************************************************/
#ifdef __KERNEL__
#define usermode_only_assert(X) 
#define usermode_only_exit(X) 
#else
#define usermode_only_assert(X) assert(X)
#define usermode_only_exit(X) exit(X)
#endif

/****************************************************************/
#define CASE(s) case s: return #s ; break

static char *convert_tag_to_string(Osd_trace_tag tag)
{
    switch (tag) {
        CASE(OSD_TRACE_ISD);

        CASE(OSD_TRACE_SO_STACK);
        CASE(OSD_TRACE_SO_DBG1);
        CASE(OSD_TRACE_SO_SENSE);
        CASE(OSD_TRACE_SO_ALL);
    
    // osd req trace levels
        CASE(OSD_TRACE_REQ);
    
        CASE(OSD_TRACE_T10);
        CASE(OSD_TRACE_SEC);
        CASE(OSD_TRACE_SIM);

        CASE(OSD_TRACE_FE);
        CASE(OSD_TRACE_PL);
        CASE(OSD_TRACE_HNS);
        
        CASE(OSD_TRACE_OSD_ALL);

    // utils trace levels
        CASE(OSD_TRACE_UTIL_DEBUG);

    // OC tags
        CASE(OSD_TRACE_OC_CRT);
        CASE(OSD_TRACE_OC_UTL);
        CASE(OSD_TRACE_OC_IO);
        CASE(OSD_TRACE_OC_FS);
        CASE(OSD_TRACE_OC_BT);
        CASE(OSD_TRACE_OC_BPT);
        CASE(OSD_TRACE_OC_SN);
        CASE(OSD_TRACE_OC_CAT);
        CASE(OSD_TRACE_OC_OCT);
        CASE(OSD_TRACE_OC_PCT);
        CASE(OSD_TRACE_OC_RM);
        CASE(OSD_TRACE_OC_SM);
        CASE(OSD_TRACE_OC_HC);
        CASE(OSD_TRACE_OC_RT);
        CASE(OSD_TRACE_OC_PM);
        CASE(OSD_TRACE_OC_JL); // TODO remove later. Adam
        CASE(OSD_TRACE_OC_LJL);
        CASE(OSD_TRACE_OC_RC);
        CASE(OSD_TRACE_OC_ALL);

    default:
        PRINT("tag %d does not have a conversion to string, exiting.\n", tag);
        usermode_only_assert(0);
    }

    /* We should not get here.
     * 
     * However, in kernel mode, I don't want to do a kernel-panic.
     */
    return "OSD_TRACE_XXX";
}

#undef CASE
/****************************************************************/

void osd_trace_init(void)
{
    int i;

    memset(trace_tag_a, 0, sizeof(trace_tag_a));
    memset(trace_tag_str_a, 0, sizeof(trace_tag_str_a));
    memset(trace_tag_str_a_prn, 0, sizeof(trace_tag_str_a_prn));
    trace_level = 0;
    level_has_been_set = 0;

    // Set the array of string-representations of tags.
    for(i=0; i<OSD_TRACE_LAST_TAG; i++) {
        char *s =convert_tag_to_string((Osd_trace_tag)i);

        usermode_only_assert(strlen(s)-10 <= (TAG_MAX_SIZE-1) &&
                             strlen(s)-10 >= TAG_MIN_SIZE );
        
        // We need to skip over the initial "OSD_TRACE_" stuff. 
        strncpy(trace_tag_str_a[i], &s[10], TAG_MAX_SIZE-1);

        // create a printable version of the tag
        memset(trace_tag_str_a_prn[i], ' ', TAG_MAX_SIZE);
        trace_tag_str_a_prn[i][0] = '<';
        memcpy(&trace_tag_str_a_prn[i][1], &s[10], strlen(&s[10]));
        trace_tag_str_a_prn[i][1+strlen(&s[10])] = '>';
        trace_tag_str_a_prn[i][TAG_MAX_SIZE-1] = 0;
    }

    // We need to make sure that the maximal level can fit in a uint8
    usermode_only_assert(LEVEL_MAX <= 255);
}

void osd_trace_init_done(void)
{
    int i;

    /* Set the trace-level per tag to the maximum between the global trace level
     * and the local trace-level. 
     */
    for (i=0; i<OSD_TRACE_LAST_TAG; i++)
        trace_tag_a[i] = MAX(trace_tag_a[i], trace_level);
}


void osd_trace_set_level(int level)
{
    if (level_has_been_set) {
        PRINT("osd_trace: the trace level has already been set, exiting.\n");
        usermode_only_exit(1);
    }
    if (!(LEVEL_MIN<= level && level <= LEVEL_MAX)) {
        PRINT("osd_trace: the trace level can be between %d and %d.\n",
              LEVEL_MIN, LEVEL_MAX);
        PRINT("           Level %d is not supported.\n", level);
        usermode_only_exit(1);
    }

    level_has_been_set = 1;
    trace_level = level;
}
    
int osd_trace_is_set(Osd_trace_tag tag, int level)
{
    return trace_tag_a[tag] >= level;
}

/****************************************************************/

void osd_trace_add_tag_lvl(Osd_trace_tag tag, int level)
{
    usermode_only_assert(0<= tag && tag < OSD_TRACE_LAST_TAG);
    usermode_only_assert(LEVEL_MIN<= level && level <= LEVEL_MAX);

    /* set the trace level [tag] to the maximum between the current
     * level, and [level].
     */
    trace_tag_a[tag] = MAX (trace_tag_a[tag], level);
}

/****************************************************************/

void osd_trace(Osd_trace_tag tag,
               int level,
               const char *format_p,
               ...)
{
    va_list args;
    char buf [200];
    
    if (trace_tag_a[tag] >= level) {
        snprintf(buf, sizeof(buf), "%s ", trace_tag_str_a_prn[tag]);
                
        va_start(args, format_p);
        // TODO: this function needs to be improved to take up less stack space. 
        vsnprintf(&buf[strlen(buf)], sizeof(buf)-strlen(buf),
		  format_p, args);
        va_end(args);

	//make sure buf is large enough
	usermode_only_assert(strlen(buf)<(sizeof(buf)-2));

#ifndef __KERNEL__
        printf(buf);
#else
        printk(buf);
#endif
    }
}

/****************************************************************/
#ifndef __KERNEL__
/* Only for user mode.
 */

/* A version of [osd_trace] that writes to a file. 
 */
void osd_trace_f(Osd_trace_tag tag,
                 int level,
                 FILE *stream_p,
                 const char *format_p,
                 ...)
{
    va_list args;
    char buf [200];
        
    if (trace_tag_a[tag] >= level) {
        snprintf(buf, sizeof(buf), "%s ", trace_tag_str_a_prn[tag]);
                
        va_start(args, format_p);
        // TODO: this function needs to be improved to take up less stack space. 
        vsnprintf(&buf[strlen(buf)], sizeof(buf)-strlen(buf),
		  format_p, args);
        va_end(args);

        fprintf(stream_p, buf);
    }
}

/* A version of [osd_trace] that takes a va_list type argument-list.
 */
void osd_trace_v(Osd_trace_tag tag,
                 int level,
                 const char *format_p,
                 va_list args)
{
    char buf [200];

    if (trace_tag_a[tag] >= level) {
        snprintf(buf, sizeof(buf), "%s ", trace_tag_str_a_prn[tag]);
        
        // TODO: this function needs to be improved to take up less stack space. 
        vsnprintf(&buf[strlen(buf)], sizeof(buf)-strlen(buf),
		  format_p, args);
        va_end(args);

        printf(buf);        
    }
}

void osd_trace_buf(Osd_trace_tag tag,
		   int level,
		   const void * buf_p,
		   int buf_len)
{
    int i;
    const unsigned char * bytes_p = (const unsigned char*)buf_p;
    if (trace_tag_a[tag] >= level) {
	if (buf_len) {
	    osd_trace(tag,level,"hex bytes [0..%d] :",buf_len-1);
	    
	    for (i=0;i<buf_len;i++) {
		if (0==(i%16)) {
		    PRINT("\n");
		    PRINT("ofs %3d : ",i);
		}
		else if (0==(i%4)) PRINT("- ");
		PRINT("%02x ",(int)bytes_p[i]);
	    }
	    PRINT("\n");
	}
	else 
	    osd_trace(tag,level,"length=0. (no values to print) \n");
    }
}

/* Add [tag_p], which is in string format, to the list of traced tags.
 * For example, to trace tag OSD_TRACE_OC_LJL with level 1 you'll need to call:
 *
 *     osd_trace_add_tag("OC_LJL", 1);
 * 
 */
static void osd_trace_add_string_tag(char *tag_p, int level)
{
    int i;

    for(i=0; i<OSD_TRACE_LAST_TAG; i++)
        if (0 == strcmp(tag_p, trace_tag_str_a[i])) {
            /* Some tags are for groups. This means that they allow tracing
             * a whole group of other tags. 
             */
            switch (i) {
            case OSD_TRACE_SO_ALL:
                osd_trace_add_tag_lvl(OSD_TRACE_SO_STACK, level);
                osd_trace_add_tag_lvl(OSD_TRACE_SO_DBG1, level);
                osd_trace_add_tag_lvl(OSD_TRACE_SO_SENSE, level);
                break;
            case OSD_TRACE_OSD_ALL:
                osd_trace_add_tag_lvl(OSD_TRACE_FE, level);
                osd_trace_add_tag_lvl(OSD_TRACE_T10, level);
                osd_trace_add_tag_lvl(OSD_TRACE_SEC, level);
                osd_trace_add_tag_lvl(OSD_TRACE_SIM, level);
                osd_trace_add_tag_lvl(OSD_TRACE_ISD, level);
                break;
            case OSD_TRACE_OC_ALL:
                osd_trace_add_tag_lvl(OSD_TRACE_OC_CRT, level);
                osd_trace_add_tag_lvl(OSD_TRACE_OC_IO, level);
                osd_trace_add_tag_lvl(OSD_TRACE_OC_FS, level);
                osd_trace_add_tag_lvl(OSD_TRACE_OC_BT, level);
                osd_trace_add_tag_lvl(OSD_TRACE_OC_BPT, level);
                osd_trace_add_tag_lvl(OSD_TRACE_OC_SN, level);
                osd_trace_add_tag_lvl(OSD_TRACE_OC_CAT, level);
                osd_trace_add_tag_lvl(OSD_TRACE_OC_OCT, level);
                osd_trace_add_tag_lvl(OSD_TRACE_OC_PCT, level);
                osd_trace_add_tag_lvl(OSD_TRACE_OC_RM, level);
                osd_trace_add_tag_lvl(OSD_TRACE_OC_SM, level);
                osd_trace_add_tag_lvl(OSD_TRACE_OC_HC, level);
                osd_trace_add_tag_lvl(OSD_TRACE_OC_RT, level);
                osd_trace_add_tag_lvl(OSD_TRACE_OC_PM, level);
                // TODO  remove later. Adam
                osd_trace_add_tag_lvl(OSD_TRACE_OC_JL, level); 
                osd_trace_add_tag_lvl(OSD_TRACE_OC_LJL, level);
                osd_trace_add_tag_lvl(OSD_TRACE_OC_RC, level);
                break;
            }

            osd_trace_add_tag_lvl((Osd_trace_tag)i, level);
            return;
        }

    printf("osd_trace: tag=%s is not supported.\n", tag_p);
    exit(1);
}

/* Add a tag, or a tag:level to the traces.
 *
 * There are two supported formats:
 *  <tag>  <tag>:<level>
 *
 * The first format is assumed to be for level=1.
 */
void osd_trace_add_string_tag_full(char *str_p)
{
    int len = strlen(str_p);

    if (len < TAG_MIN_SIZE) {
        PRINT("osd_trace: Tag too short, has to be at least of size %d\n", TAG_MIN_SIZE);
        exit(1);
    }
    if (len >= TAG_MAX_SIZE) {
        PRINT("osd_trace: Tag too long, has to be shorter than %d\n", TAG_MAX_SIZE);
        exit(1);
    }

    // We check if this string combines a tag and a level
    if (str_p[len-2] == ':') {
        // Complex case, this is (tag:level) pair
        char lcopy[TAG_MAX_SIZE];
        int level;
        
        // split the string into two sub-strings
        memset(lcopy, 0, sizeof(lcopy));
        strncpy(lcopy, str_p, sizeof(lcopy)-1);
        lcopy[len-2] = '\0';

        // handle the level part
        level = atoi((char*)&lcopy[len-1]);
        if (!(level <= LEVEL_MAX && level >= LEVEL_MIN)) {
            printf("osd_trace: Level %d not supported\n", level);
            exit(1);
        }
        
        // handle the tag part
        osd_trace_add_string_tag(lcopy, level);
        
    } else {
        // Regular case, this is a simple tag
        osd_trace_add_string_tag(str_p, 1);
    }
}

void osd_trace_print_tag_list(void)
{
    int i;

    printf("    The set of possible tags is:\n");
    for (i=0; i<OSD_TRACE_LAST_TAG; i++) 
        printf("\t %s\n", trace_tag_str_a[i]);
    printf("    A level per-tag can be specified with <tag:level>\n");
    printf("    For example:\n");
    printf("\t OC_JL:2 \n");
    printf("    will trace tag OC_JL at levels 1 and 2\n");
}

#endif

/****************************************************************/
