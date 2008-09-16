#!/usr/bin/python
#
# Initialize OSDs for use in PVFS.  Only call this from whatever startup
# script is used to build the fs.conf and start the OSDs.  It has a messy
# command line, rather than trying to parse a fs.conf.
#
# Copyright (C) 2007 Pete Wyckoff <pw@osc.edu>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import sys
from pyosd import *

# magic constants from pvfs/src/client/sysint/osd.h

# Pages for object and directory attributes
PVFS_USEROBJECT_DIR_PG  = LUN_PG_LB + 100
PVFS_USEROBJECT_ATTR_PG = LUN_PG_LB + 101
# The partitions; one for datafiles, another for metafiles and dir objects
PVFS_OSD_DATA_PID = 0x10003
PVFS_OSD_META_PID = 0x10004
# Attribute location of the fs.conf text, on root handle only
PVFS_USEROBJECT_FSCONF_PAGE   = LUN_PG_LB + 200
PVFS_USEROBJECT_FSCONF_NUMBER =   1
# PVFS code for uid | gid | perm | ...
PVFS_ATTR_COMMON_ALL = 0x7f
# PVFS code for directory object type
PVFS_TYPE_DIRECTORY = 1 << 2

def hexdump(s, len):
	buf = ''
	for i in range((len+7)/8):
		buf = buf + "%02x:" % (i*8)
		for j in range(min(8, len - i*8)):
			buf = buf + " %02x" % (ord(s[i*8+j]))
		buf = buf + "\n"
	return buf

# submit, check status, print sense, or resolve attributes, returning them
def run(command):
	dev.submit_and_wait(command)
	if command.status != 0:
		print "Command failed:", command.show_sense(),
		assert 0 == 1
	return command.attr_resolve()

def create_any_object(pid):
	command = OSDCommand().set_create(pid)
	command.attr_build(OSDAttr(ATTR_GET, CUR_CMD_ATTR_PG, CCAP_OID, \
				   CCAP_OID_LEN))
	attr = run(command)
	return ntohll(attr.val)

def usage():
	print >>sys.stderr, "Usage:", sys.argv[0], \
	                    "<target name> <datalb> <metalb>", \
			    "[<root handle> <fs.conf>]"
	sys.exit(1)

set_progname(sys.argv[0])

root_handle = 0
if len(sys.argv) == 4 or len(sys.argv) == 6:
	devname = sys.argv[1];
	datalb = int(sys.argv[2]);
	metalb = int(sys.argv[3]);
	if len(sys.argv) == 6:
		root_handle = int(sys.argv[4])
		fsconf = sys.argv[5]
else:
	usage()

drives = OSDDriveList()
dev = False
for d in drives:
	if d.targetname == devname:
		dev = OSDDevice(d.chardev)
		break
if not dev:
	print >>sys.stderr, "Drive", devname, "not found."
	sys.exit(1)

# format
run(OSDCommand().set_format_osd(1<<30))

# Set lower bounds in the data and meta spaces to fake allocation of
# handles in a specified range.  Trust that the target will just add
# one to the highest allocated handle and return that, and do not
# allocate and free so many that it would wrap.
run(OSDCommand().set_create_partition(PVFS_OSD_DATA_PID))
run(OSDCommand().set_create(PVFS_OSD_DATA_PID, datalb))

run(OSDCommand().set_create_partition(PVFS_OSD_META_PID))
if metalb != root_handle:
	run(OSDCommand().set_create(PVFS_OSD_META_PID, metalb))

# create root handle, the top level directory
# add fs.conf text as an attribute on the root handle
if root_handle:
	fh = open(fsconf, "r")
	buf = fh.read()
	fh.close()
	pid = PVFS_OSD_META_PID
	oid = root_handle
	command = OSDCommand().set_create(pid, oid)
	command.attr_build([ \
		OSDAttr(ATTR_SET, PVFS_USEROBJECT_ATTR_PG, 0, 0), \
		OSDAttr(ATTR_SET, PVFS_USEROBJECT_ATTR_PG, 1, 0), \
		OSDAttr(ATTR_SET, PVFS_USEROBJECT_ATTR_PG, 2, 0777), \
		OSDAttr(ATTR_SET, PVFS_USEROBJECT_ATTR_PG, 3, \
			PVFS_ATTR_COMMON_ALL), \
		OSDAttr(ATTR_SET, PVFS_USEROBJECT_ATTR_PG, 4, \
			PVFS_TYPE_DIRECTORY), \
		OSDAttr(ATTR_SET, PVFS_USEROBJECT_FSCONF_PAGE, \
			PVFS_USEROBJECT_FSCONF_NUMBER, buf), \
	])
	run(command)

