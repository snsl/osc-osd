#!/usr/bin/python
#
# osd-target/tests/osd-test rewritten in python interface
#
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

def ccap_verify(ccap, len, pid, oid):
	assert len == CCAP_TOTAL_LEN
	assert ntohl(ccap[0:4]) == CUR_CMD_ATTR_PG
	assert ntohl(ccap[4:8]) == len - 8
	assert ntohll(ccap[32:40]) == pid
	assert ntohll(ccap[40:48]) == oid + 5 - 1
	assert ntohll(ccap[48:56]) == 0

def test_osd_format():
	run(OSDCommand().set_format_osd(1<<30))

def test_osd_create():
	# invalid pid/oid, test must fail
	runfail(OSDCommand().set_create(0, 1))

	# invalid oid test must fail
	runfail(OSDCommand().set_create(USEROBJECT_PID_LB, 1))

	# num > 1 cannot request oid, test must fail
	runfail(OSDCommand().set_create(USEROBJECT_PID_LB, USEROBJECT_OID_LB, \
					2))

	run(OSDCommand().set_create_partition(PARTITION_PID_LB))

	run(OSDCommand().set_create(USEROBJECT_PID_LB, USEROBJECT_OID_LB))

	run(OSDCommand().set_remove(USEROBJECT_PID_LB, USEROBJECT_OID_LB))

	# remove non-existing object, test must fail
	runfail(OSDCommand().set_remove(USEROBJECT_PID_LB, USEROBJECT_OID_LB))

	run(OSDCommand().set_remove_partition(PARTITION_PID_LB))

def test_osd_set_attributes():
	run(OSDCommand().set_create_partition(PARTITION_PID_LB))
	run(OSDCommand().set_create(USEROBJECT_PID_LB, USEROBJECT_OID_LB))

	# setting root attr must fail (need to use list format here, as
	# 0 in the set for page format means do nothing)
	oneattr = OSDAttr(ATTR_SET, 0, 0, "")
	command = OSDCommand().set_set_attributes(ROOT_PID, ROOT_OID)
	runfail(command.attr_build(oneattr))

	# unsettable page modification must fail
	command = OSDCommand().set_set_attributes(PARTITION_PID_LB, \
						  PARTITION_OID)
	runfail(command.attr_build(oneattr))

	# unsettable collection page must fail
	command = OSDCommand().set_set_attributes(COLLECTION_PID_LB, \
						  COLLECTION_OID_LB)
	runfail(command.attr_build(oneattr))

	# unsettable userobject page must fail
	command = OSDCommand().set_set_attributes(USEROBJECT_PID_LB, \
						  USEROBJECT_OID_LB)
	runfail(command.attr_build(oneattr))

	# info attr > 40 bytes, test must fail
	command = OSDCommand().set_set_attributes(USEROBJECT_PID_LB, \
						  USEROBJECT_OID_LB)
	attr = OSDAttr(ATTR_SET, USEROBJECT_PG + LUN_PG_LB, 0, \
		       "This is test, long test more than forty bytes")
	runfail(command.attr_build(attr))

	# this test is normal setattr, must succeed
	command = OSDCommand().set_set_attributes(USEROBJECT_PID_LB, \
						  USEROBJECT_OID_LB)
	attr = OSDAttr(ATTR_SET, USEROBJECT_PG + LUN_PG_LB + 1, 1, \
		       "Madhuri Dixit")
	run(command.attr_build(attr))

	run(OSDCommand().set_remove(USEROBJECT_PID_LB, USEROBJECT_OID_LB))
	run(OSDCommand().set_remove_partition(PARTITION_PID_LB))

def test_osd_read_write():
	run(OSDCommand().set_create_partition(PARTITION_PID_LB))
	run(OSDCommand().set_create(USEROBJECT_PID_LB, USEROBJECT_OID_LB))

	s = "Hello World! Get life\n"
	run(OSDCommand().set_write(USEROBJECT_PID_LB, USEROBJECT_OID_LB, s))

	command = OSDCommand().set_read(USEROBJECT_PID_LB, USEROBJECT_OID_LB, \
				        100)
	run(command)
	assert command.indata == s

	run(OSDCommand().set_remove(USEROBJECT_PID_LB, USEROBJECT_OID_LB))
	run(OSDCommand().set_remove_partition(PARTITION_PID_LB))

def test_osd_create_partition():
	run(OSDCommand().set_create_partition(0))

	# create dup pid must fail
	run(OSDCommand().set_create_partition(PARTITION_PID_LB + 1))
	runfail(OSDCommand().set_create_partition(PARTITION_PID_LB + 1))

	run(OSDCommand().set_remove_partition(PARTITION_PID_LB))
	run(OSDCommand().set_remove_partition(PARTITION_PID_LB + 1))
	# XXX: this works (missing pid), but should not
	run(OSDCommand().set_remove_partition(PARTITION_PID_LB + 2))

def test_osd_get_ccap(pid, oid):
	command = OSDCommand().set_get_attributes(pid, oid)
	command.attr_build(OSDAttr(ATTR_GET_PAGE, CUR_CMD_ATTR_PG, 1024))
	attr = run(command)
	assert attr.outlen == CCAP_TOTAL_LEN
	assert ntohl(attr.val[0:4]) == CUR_CMD_ATTR_PG
	assert ntohl(attr.val[4:8]) == CCAP_TOTAL_LEN - 8
	assert ntohll(attr.val[32:40]) == pid
	assert ntohll(attr.val[40:48]) == oid
	assert ntohll(attr.val[48:56]) == 0

def ntoh_time(buf):
	return (ntohs(buf[0:2]) << 32) | ntohl(buf[2:6])

def test_osd_get_utsap():
	s = "Hello World! Get life blah blah blah\n"
	run(OSDCommand().set_write(USEROBJECT_PID_LB, USEROBJECT_OID_LB, s))
	command = OSDCommand().set_read(USEROBJECT_PID_LB, USEROBJECT_OID_LB, \
					100)
	run(command)
	assert command.indata == s

	command = OSDCommand().set_set_attributes(USEROBJECT_PID_LB, \
						  USEROBJECT_OID_LB)
	command.attr_build(OSDAttr(ATTR_SET, USEROBJECT_PG + LUN_PG_LB, 2, s))
	run(command)

	command = OSDCommand().set_get_attributes(USEROBJECT_PID_LB, \
						  USEROBJECT_OID_LB)
	command.attr_build(OSDAttr(ATTR_GET_PAGE, USER_TMSTMP_PG, 100))
	attr = run(command)

	assert ntohl(attr.val[0:4]) == USER_TMSTMP_PG
	assert ntohl(attr.val[4:8]) == UTSAP_TOTAL_LEN - 8

	# XXX: these should be different, but they are not.
	atime = ntoh_time(attr.val[26:32])
	mtime = ntoh_time(attr.val[32:38])
	assert atime != 0 and mtime != 0 and mtime <= atime

	atime = ntoh_time(attr.val[14:20])
	mtime = ntoh_time(attr.val[20:26])
	assert atime != 0 and mtime != 0 and mtime == atime

def test_osd_get_attributes():
	run(OSDCommand().set_create_partition(PARTITION_PID_LB))
	run(OSDCommand().set_create(USEROBJECT_PID_LB, USEROBJECT_OID_LB))

	s = "Madhuri Dixit"
	command = OSDCommand().set_set_attributes(USEROBJECT_PID_LB, \
						  USEROBJECT_OID_LB)
	command.attr_build(OSDAttr(ATTR_SET, USEROBJECT_PG + LUN_PG_LB, 1, s))
	run(command)

	command = OSDCommand().set_get_attributes(USEROBJECT_PID_LB, \
						  USEROBJECT_OID_LB)
	command.attr_build(OSDAttr(ATTR_GET, USEROBJECT_PG + LUN_PG_LB, 1, 100))
	attr = run(command)
	assert attr.val == s

	# run different get attr tests
	test_osd_get_ccap(USEROBJECT_PID_LB, USEROBJECT_OID_LB)
	test_osd_get_utsap()

	run(OSDCommand().set_remove(USEROBJECT_PID_LB, USEROBJECT_OID_LB))
	run(OSDCommand().set_remove_partition(PARTITION_PID_LB))

set_progname(sys.argv[0])
drives = OSDDriveList()
if len(drives) < 1:
	print >>sys.stderr, "No drives."
	sys.exit(1)

dev = OSDDevice(drives[0].chardev)

test_osd_format()
test_osd_create()
test_osd_set_attributes()
test_osd_read_write()
test_osd_create_partition()
test_osd_get_attributes()

