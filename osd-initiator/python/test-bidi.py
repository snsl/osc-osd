#!/usr/bin/python
#
# Do things that force bidirectional commands.
#
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
	#print "buf1"
	#print hexdump(buf1, len(buf1))
	#print "buf2"
	#print hexdump(buf2, len(buf2))
	for i in range(len(buf1)):
		if buf1[i] != buf2[i]:
			print >>sys.stderr, \
			   "Bufs differ at offset %d: expected %02x, got %02x" \
			   % (i, ord(buf1[i]), ord(buf2[i]))
			sys.exit(1)

def ccap_verify(ccap, len, pid, oid):
	#print "ccap"
	#print hexdump(ccap, len)
	assert len == CCAP_TOTAL_LEN
	assert ntohl(ccap[0:4]) == CUR_CMD_ATTR_PG
	assert ntohl(ccap[4:8]) == len - 8
	assert ntohll(ccap[32:40]) == pid
	assert ntohll(ccap[40:48]) == oid
	assert ntohll(ccap[48:56]) == 0

def test_one_bidi(pid, oid, page, number, buf, size, attrbuf, attrsize):
	print "Write"
	command = OSDCommand()
	command.set_write(pid, oid, buf)
	run(command)

	print "Read"
	command = OSDCommand()
	command.set_read(pid, oid, size)
	run(command)
	compare(buf, command.indata)

	print "Setattr (one)"
	command = OSDCommand()
	command.set_set_attributes(pid, oid)
	command.attr_build(OSDAttr(ATTR_SET, page, number, attrbuf))
	run(command)

	print "Setattr (list)"
	command = OSDCommand()
	command.set_set_attributes(pid, oid)
	command.attr_build([ \
		OSDAttr(ATTR_SET, page, number, attrbuf), \
		OSDAttr(ATTR_SET, page, number + 1, attrbuf)])
	run(command)

	print "Getattr (page)"
	command = OSDCommand()
	command.set_get_attributes(pid, oid)
	command.attr_build(OSDAttr(ATTR_GET_PAGE, CUR_CMD_ATTR_PG, \
				   CCAP_TOTAL_LEN))
	attr = run(command)
	ccap_verify(attr.val, attr.outlen, pid, oid)

	print "Getattr (list)"
	command = OSDCommand()
	command.set_get_attributes(pid, oid)
	command.attr_build(OSDAttr(ATTR_GET, page, number, attrsize))
	attr = run(command)
	compare(attrbuf, attr.val)

	print "Write and getattr (page)"
	command = OSDCommand()
	command.set_write(pid, oid, buf)
	command.attr_build(OSDAttr(ATTR_GET_PAGE, CUR_CMD_ATTR_PG, \
				   CCAP_TOTAL_LEN))
	attr = run(command)
	ccap_verify(attr.val, attr.outlen, pid, oid)

	print "Write and getattr (list)"
	command = OSDCommand()
	command.set_write(pid, oid, buf)
	command.attr_build(OSDAttr(ATTR_GET, page, number, attrsize))
	attr = run(command)
	compare(attrbuf, attr.val)

	print "Write and setattr (one)"
	command = OSDCommand()
	command.set_write(pid, oid, buf)
	command.attr_build(OSDAttr(ATTR_SET, page, number, attrbuf))
	run(command)

	print "Write and setattr (list)"
	command = OSDCommand()
	command.set_write(pid, oid, buf)
	command.attr_build([ \
		OSDAttr(ATTR_SET, page, number, attrbuf), \
		OSDAttr(ATTR_SET, page, number + 1, attrbuf)])
	run(command)

	print "Read and getattr (page)"
	command = OSDCommand()
	command.set_read(pid, oid, size)
	command.attr_build(OSDAttr(ATTR_GET_PAGE, CUR_CMD_ATTR_PG, \
				   CCAP_TOTAL_LEN))
	attr = run(command)
	compare(buf, command.indata)
	ccap_verify(attr.val, attr.outlen, pid, oid)

	print "Read and getattr (list)"
	command = OSDCommand()
	command.set_read(pid, oid, size)
	command.attr_build(OSDAttr(ATTR_GET, page, number, attrsize))
	attr = run(command)
	compare(buf, command.indata)
	compare(attrbuf, attr.val)

	print "Read and setattr (one)"
	command = OSDCommand()
	command.set_read(pid, oid, size)
	command.attr_build(OSDAttr(ATTR_SET, page, number, attrbuf))
	run(command)
	compare(buf, command.indata)

	print "Read and setattr (list)"
	command = OSDCommand()
	command.set_read(pid, oid, size)
	command.attr_build([ \
		OSDAttr(ATTR_SET, page, number, attrbuf), \
		OSDAttr(ATTR_SET, page, number + 1, attrbuf)])
	run(command)
	compare(buf, command.indata)


def test_bidi(pid, oid):
	page = USEROBJECT_PG + LUN_PG_LB
	number = 27

	#for size in [ 2, 4096, 8192, 65534, 262144 ]:
	for size in [ 2 ]:
		buf = build_buf(size)
		#for attrsize in [ 2, 4096, 8192, 65534 ]:
		for attrsize in [ 65534 ]:
			attrbuf = build_buf(attrsize)

			print "Buf size", size, "attrbuf size", attrsize
			test_one_bidi(pid, oid, page, number, buf, size, \
				      attrbuf, attrsize)

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

test_bidi(pid, oid)

