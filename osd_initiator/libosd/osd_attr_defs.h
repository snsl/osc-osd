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
#ifndef _OSD_ATTR_DEFS
#define _OSD_ATTR_DEFS

//attr_num=wildcard means all page attributes are accessed
//page num=wildcard meand all object associated pages are accessed
#define OSD_ATTR_WILDCARD   0xFFFFFFFF 

//OSD_ATTR_UNDEFINED_VALUE_LENGTH is sent as the atttribute length
//for atttributes whose value is undefined.
//such length isn't followed by a value (as if it was zero length)
#define OSD_ATTR_UNDEFINED_VAL_LENGTH 0xFFFF

//constants used by paeg number definitions in source for generated code
#define PAGE_CLS_MSK ((uint32_t)0xF0000000)
#define PAGE_P ((uint32_t)0x30000000)
#define PAGE_C ((uint32_t)0x60000000)
#define PAGE_R ((uint32_t)0x90000000)
#define PAGE_A ((uint32_t)0xF0000000)
#define PAGE_U ((uint32_t)0)

#define PAGE_T_STD ((uint32_t)0)
#define PAGE_T_USR ((uint32_t)0x10000000)
#define PAGE_T_VND ((uint32_t)0x20000000)

//auto generated code with all page numbers and attribute numbers
#include "osd_attr_numbers.h"
#endif
