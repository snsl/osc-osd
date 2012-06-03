#!/usr/bin/python
#
# Make sure data gets read and written correctly, for sizes big enough
# to force RDMA in the iSER case.
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
import random
from pyosd import *

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

def build_buf(size):
	off = random.randint(0, 255)
	buf = ''
	for i in range(size):
		buf += chr(off)
		off += 1
		if off >= 256:
			off = 0
	return buf

def compare(buf1, buf2):
	for i in range(len(buf1)):
		if buf1[i] != buf2[i]:
			print >>sys.stderr, \
			    "Bufs differ at offset %d: %02x vs %02x" \
			    % (i, ord(buf1[i]), ord(buf2[i]))
			sys.exit(1)

def test_rw(pid, oid):
	for size in map(lambda x: 1 << x, range(1, 19)) \
		    + [ (1<<18) + (1<<17) ]:
		print "Test size", size
		buf = build_buf(size)
		command = OSDCommand()
		command.set_write(pid, oid, buf)
		run(command)

		command = OSDCommand()
		command.set_read(pid, oid, size)
		run(command)
		compare(buf, command.indata)

set_progname(sys.argv[0])
random.seed()
drives = OSDDriveList()
if len(drives) < 1:
	print >>sys.stderr, "No drives."
	sys.exit(1)

dev = OSDDevice(drives[0].chardev)

pid = PARTITION_PID_LB
oid = USEROBJECT_OID_LB

run(OSDCommand().set_format_osd(1<<30))
run(OSDCommand().set_create_partition(oid))
run(OSDCommand().set_create(oid))

test_rw(pid, oid)

