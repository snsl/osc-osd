#!/usr/bin/python
#
# Test collections.
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

def runfail(command):
	dev.submit_and_wait(command)
	if command.status == 0:
		print "Command was supposed to fail."
		assert 0 == 1
	command.attr_resolve()

def create_any_collection(pid):
	command = OSDCommand().set_create_collection(pid)
	command.attr_build(OSDAttr(ATTR_GET, CUR_CMD_ATTR_PG, CCAP_OID, \
				   CCAP_OID_LEN))
	attr = run(command)
	return ntohll(attr.val)

def create_any_object(pid):
	command = OSDCommand().set_create(pid)
	command.attr_build(OSDAttr(ATTR_GET, CUR_CMD_ATTR_PG, CCAP_OID, \
				   CCAP_OID_LEN))
	attr = run(command)
	return ntohll(attr.val)

set_progname(sys.argv[0])
drives = OSDDriveList()
if len(drives) < 1:
	print >>sys.stderr, "No drives."
	sys.exit(1)

dev = OSDDevice(drives[0].chardev)

run(OSDCommand().set_format_osd(1<<30))
pid = PARTITION_PID_LB
run(OSDCommand().set_create_partition(pid))

# basic create/remove
cid = COLLECTION_OID_LB
run(OSDCommand().set_create_collection(pid, cid))
run(OSDCommand().set_remove_collection(pid, cid))

# with an object that gets in the way
oid = USEROBJECT_OID_LB
run(OSDCommand().set_create(pid, oid))
runfail(OSDCommand().set_create_collection(pid, oid))
run(OSDCommand().set_remove(pid, oid))

# with a collection that gets in the way
cid = COLLECTION_OID_LB
run(OSDCommand().set_create_collection(pid, cid))
runfail(OSDCommand().set_create_collection(pid, cid))
run(OSDCommand().set_remove_collection(pid, cid))

# create, target chooses
cid = create_any_collection(pid)
run(OSDCommand().set_remove_collection(pid, cid))

# put objects into and out of collections
cid1 = create_any_collection(pid)
cid2 = create_any_collection(pid)
cid3 = create_any_collection(pid)
oid = create_any_object(pid)
command = OSDCommand().set_set_attributes(pid, oid)
command.attr_build(OSDAttr(ATTR_SET, USER_COLL_PG, 1, htonll(cid1)))
run(command)
command = OSDCommand().set_set_attributes(pid, oid)
command.attr_build(OSDAttr(ATTR_SET, USER_COLL_PG, 2, htonll(cid2)))
run(command)
# replace existing one
command = OSDCommand().set_set_attributes(pid, oid)
command.attr_build(OSDAttr(ATTR_SET, USER_COLL_PG, 2, htonll(cid3)))
run(command)
# can't remove if something is in it
runfail(OSDCommand().set_remove_collection(pid, cid3))
# delete one
command = OSDCommand().set_set_attributes(pid, oid)
command.attr_build(OSDAttr(ATTR_SET, USER_COLL_PG, 2, None))
run(command)
# set same collection twice
command = OSDCommand().set_set_attributes(pid, oid)
command.attr_build(OSDAttr(ATTR_SET, USER_COLL_PG, 3, htonll(cid1)))
runfail(command)
# remove this one
command = OSDCommand().set_set_attributes(pid, oid)
command.attr_build(OSDAttr(ATTR_SET, USER_COLL_PG, 1, None))
run(command)
run(OSDCommand().set_remove_collection(pid, cid1))
run(OSDCommand().set_remove_collection(pid, cid2))
run(OSDCommand().set_remove_collection(pid, cid3))
run(OSDCommand().set_remove(pid, oid))

# remove a collection with something in it
cid = create_any_collection(pid)
oid = create_any_object(pid)
command = OSDCommand().set_set_attributes(pid, oid)
command.attr_build(OSDAttr(ATTR_SET, USER_COLL_PG, cid, htonll(cid)))
run(command)
runfail(OSDCommand().set_remove_collection(pid, cid))
run(OSDCommand().set_remove(pid, oid))
run(OSDCommand().set_remove_collection(pid, cid))

# remove a collection with something in it, using force flag
cid = create_any_collection(pid)
oid = create_any_object(pid)
command = OSDCommand().set_set_attributes(pid, oid)
command.attr_build(OSDAttr(ATTR_SET, USER_COLL_PG, cid, htonll(cid)))
run(command)
run(OSDCommand().set_remove_collection(pid, cid, 1))
run(OSDCommand().set_remove(pid, oid))
runfail(OSDCommand().set_remove_collection(pid, cid))

# get member attributes, currently unimplemented in target
cid = create_any_collection(pid)
oid1 = create_any_object(pid)
oid2 = create_any_object(pid)
s1 = "hiya"
s2 = "himon"
run(OSDCommand().set_write(pid, oid1, s1))
run(OSDCommand().set_write(pid, oid2, s2))
command = OSDCommand().set_set_attributes(pid, oid1)
command.attr_build(OSDAttr(ATTR_SET, USER_COLL_PG, cid, htonll(cid)))
run(command)
command = OSDCommand().set_set_attributes(pid, oid2)
command.attr_build(OSDAttr(ATTR_SET, USER_COLL_PG, cid, htonll(cid)))
run(command)
command = OSDCommand().set_get_member_attributes(pid, cid)
command.attr_build(OSDAttr(ATTR_GET, USER_INFO_PG, UIAP_LOGICAL_LEN, 8))
attr = run(command)
assert attr[0].val == len(s1)
assert attr[1].val == len(s2)
run(OSDCommand().set_remove(pid, oid1))
run(OSDCommand().set_remove(pid, oid2))
run(OSDCommand().set_remove_collection(pid, cid))

# set member attributes
cid = create_any_collection(pid)
oid1 = create_any_object(pid)
oid2 = create_any_object(pid)
command = OSDCommand().set_attributes(pid, oid1)
command.attr_build(OSDAttr(ATTR_SET, USER_COLL_PG, cid, htonll(cid)))
run(command)
command = OSDCommand().set_attributes(pid, oid2)
command.attr_build(OSDAttr(ATTR_SET, USER_COLL_PG, cid, htonll(cid)))
run(command)
s = "hello"
command = OSDCommand().get_member_attributes(pid, cid)
command.attr_build(OSDAttr(ATTR_SET, LUN_PG_LB+5, 26, s))
attr = run(command)
command = OSDCommand().get_attributes(pid, oid1)
command.attr_build(OSDAttr(ATTR_GET, LUN_PG_LB+5, 26, 100))
attr = run(command)
assert attr.val == s
command = OSDCommand().get_attributes(pid, oid2)
command.attr_build(OSDAttr(ATTR_GET, LUN_PG_LB+5, 26, 100))
attr = run(command)
assert attr.val == s
run(OSDCommand().set_remove(pid, oid1))
run(OSDCommand().set_remove(pid, oid2))
run(OSDCommand().set_remove_collection(pid, cid))

# list collection

