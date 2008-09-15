#!/usr/bin/python
#
# osd-target/tests/cdb-test rewritten in python interface
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

def ccap_verify(ccap, len, pid, oid):
	assert len == CCAP_TOTAL_LEN
	assert ntohl(ccap[0:4]) == CUR_CMD_ATTR_PG
	assert ntohl(ccap[4:8]) == len - 8
	assert ntohll(ccap[32:40]) == pid
	assert ntohll(ccap[40:48]) == oid + 5 - 1
	assert ntohll(ccap[48:56]) == 0

def test_partition():
	# create partition + empty getpage_setlist
	run(OSDCommand().set_create_partition(PARTITION_PID_LB))

	# remove partition + empty getpage_setlist
	run(OSDCommand().set_remove_partition(PARTITION_PID_LB))

	# create partition + empty getlist_setlist
	command = OSDCommand().set_create_partition(PARTITION_PID_LB)
	command.attr_build(OSDAttr(ATTR_GET_PAGE, 0, 0))
	run(command)

	# remove partition + empty getpage_setlist
	command = OSDCommand().set_remove_partition(PARTITION_PID_LB)
	command.attr_build(OSDAttr(ATTR_GET_PAGE, 0, 0))
	run(command)

def test_create():
	# create partition + empty getpage_setlist
	run(OSDCommand().set_create_partition(PARTITION_PID_LB))

	# create 1 object
	run(OSDCommand().set_create(USEROBJECT_PID_LB, USEROBJECT_OID_LB))

	# remove the object
	run(OSDCommand().set_remove(USEROBJECT_PID_LB, USEROBJECT_OID_LB))

	# create 5 objects & get ccap
	command = OSDCommand().set_create(USEROBJECT_PID_LB, 0, 5)
	command.attr_build(OSDAttr(ATTR_GET_PAGE, CUR_CMD_ATTR_PG,\
				   CCAP_TOTAL_LEN))
	attr = run(command)
	ccap_verify(attr.val, attr.outlen, USEROBJECT_PID_LB, USEROBJECT_OID_LB)

	# remove 5 objects
	for i in range(5):
		run(OSDCommand().set_remove(USEROBJECT_PID_LB, \
					    USEROBJECT_OID_LB + i))

	# create 5 objects, set 2 attr on each
	command = OSDCommand().set_create(USEROBJECT_PID_LB, 0, 5)
	command.attr_build([OSDAttr(ATTR_SET, USEROBJECT_PG + LUN_PG_LB, \
				    111, "Madhuri Dixit Rocks!!"), \
	                    OSDAttr(ATTR_SET, USEROBJECT_PG + LUN_PG_LB + 1, \
				    321, "A cigarrete a day etc.")])
	run(command)

	# remove 5 objects and get previously set attributes for each
	attr = [ OSDAttr(ATTR_GET, USEROBJECT_PG + LUN_PG_LB + 1, 321, 100), \
		 OSDAttr(ATTR_GET, USEROBJECT_PG + LUN_PG_LB, 111, 100) ]
	for i in range(5):
		command = OSDCommand()
		command.set_remove(USEROBJECT_PID_LB, USEROBJECT_OID_LB + i)
		attr = run(command.attr_build(attr))
		assert attr[0].val == "A cigarrete a day etc."
		assert attr[1].val == "Madhuri Dixit Rocks!!"

	# remove partition
	run(OSDCommand().set_remove_partition(USEROBJECT_PID_LB))


set_progname(sys.argv[0])
drives = OSDDriveList()
if len(drives) < 1:
	print >>sys.stderr, "No drives."
	sys.exit(1)

dev = OSDDevice(drives[0].chardev)

run(OSDCommand().set_format_osd(1<<30))

test_partition()
test_create()

