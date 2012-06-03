#!/usr/bin/python
#
# test program for python interface
#
# Copyright (C) 2007 OSD Team <pvfs-osd@osc.edu>
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
import pyosd

def hexdump(s, len):
	buf = ''
	for i in range((len+7)/8):
		buf = buf + "%02x:" % (i*8)
		for j in range(min(8, len - i*8)):
			buf = buf + " %02x" % (ord(s[i*8+j]))
		buf = buf + "\n"
	return buf

# for error reporting
pyosd.set_progname(sys.argv[0])

# find and show all the drives
drives = pyosd.OSDDriveList()
if len(drives) < 1:
	print >>sys.stderr, "No drives."
	sys.exit(1)

print "Available drives:"
for i in range(len(drives)):
	print "    drive", i, "name", drives[i].targetname, \
	      "device", drives[i].chardev

# open the first device
dev = pyosd.OSDDevice()
dev.open(drives[0].chardev)

# inquiry
print "inquiry"
command = pyosd.OSDCommand()
command.set_inquiry()
dev.submit_and_wait(command)
if command.status:
	print "status", command.status
	print command.show_sense(),
else:
	print "inquiry indata (%d):" % command.inlen
	if command.inlen:
		print hexdump(command.indata, command.inlen),

# format
print "format"
command = pyosd.OSDCommand()
command.set_format_osd(1 << 30)
dev.submit_and_wait(command)
if command.status:
	print "status", command.status
	print command.show_sense(),

pid = 0x10000

# create paritition
print "create partition"
command = pyosd.OSDCommand()
command.set_create_partition(pid)
dev.submit_and_wait(command)
if command.status:
	print "status", command.status
	print command.show_sense(),

oid = 0x10010

# create particular oid
print "create particular object"
command = pyosd.OSDCommand()
command.set_create(pid, oid)
dev.submit_and_wait(command)
if command.status:
	print "status", command.status
	print command.show_sense(),

ccap_page = 0xfffffffe
ccap_number_oid = 4

uiap_page = 0x1
uiap_number_len = 0x82

# create any oid, ask for result
print "create any object"
command = pyosd.OSDCommand()
command.set_create(pid)
attr = pyosd.OSDAttr(pyosd.ATTR_GET, ccap_page, ccap_number_oid, 8)
command.attr_build(attr)
dev.submit_and_wait(command)
if command.status:
	print "status", command.status
	print command.show_sense(),
else:
	attr = command.attr_resolve()
	oid = pyosd.ntohll(attr.val)
	print "created oid", oid

# write it
print "write that object"
command = pyosd.OSDCommand()
command.set_write(pid, oid, "Some data from python.")
dev.submit_and_wait(command)
if command.status:
	print "status", command.status
	print command.show_sense(),

# two attrs
print "two getattr"
command = pyosd.OSDCommand()
command.set_get_attributes(pid, oid)
attr = [ pyosd.OSDAttr(pyosd.ATTR_GET, ccap_page, ccap_number_oid, 8), \
         pyosd.OSDAttr(pyosd.ATTR_GET, uiap_page, uiap_number_len, 8) ]
command.attr_build(attr)
dev.submit_and_wait(command)
if command.status:
	print "status", command.status
	print command.show_sense(),
else:
	attr = command.attr_resolve()
	verify_oid = pyosd.ntohll(attr[0].val)
	len = pyosd.ntohll(attr[1].val)
	print "should be same oid:", oid, "has len", len

# read it
command = pyosd.OSDCommand()
command.set_read(pid, oid, 1024)
dev.submit_and_wait(command)
good = 1
if command.status:
	good = 0
	if command.status == 2 and command.sense_key == 1 \
	   and command.sense_code == 0x3b17:
	   	print "expected this sense:", command.show_sense(),
		good = 1
	else:
	    print "status", command.status
	    print command.show_sense(),
if good:
	print "read:", command.indata

# two set attrs
print "two setattr"
command = pyosd.OSDCommand()
command.set_set_attributes(pid, oid)
attr = [ pyosd.OSDAttr(pyosd.ATTR_SET, 0x10000, 12, "testattr1"), \
         pyosd.OSDAttr(pyosd.ATTR_SET, 0x10201, 18, "testattr2") ]
command.attr_build(attr)
dev.submit_and_wait(command)
if command.status:
	print "status", command.status
	print command.show_sense(),
else:
	command.attr_resolve()
	print "attrs set"

# two set attrs
print "read back two setattr (and one undefined)"
command = pyosd.OSDCommand()
command.set_get_attributes(pid, oid)
attr = [ pyosd.OSDAttr(pyosd.ATTR_GET, 0x10201, 18, 40), \
         pyosd.OSDAttr(pyosd.ATTR_GET, 0x10000, 12, 40), \
	 pyosd.OSDAttr(pyosd.ATTR_GET, 0x10999, 14, 40) ]
command.attr_build(attr)
dev.submit_and_wait(command)
if command.status:
	print "status", command.status
	print command.show_sense(),
else:
	attr = command.attr_resolve()
	print "got attrs", attr[0].val, "and", attr[1].val, "and", \
	      attr[2].val

dev.close()

