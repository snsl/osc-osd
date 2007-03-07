#!/usr/bin/python
#
# test program for python interface
#
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
print "inquiry status", command.status
print "inquiry indata (%d):" % command.inlen
if command.inlen:
	print hexdump(command.indata, command.inlen),
# XXX: how to free indata on command?

# format
print "format"
command = pyosd.OSDCommand()
command.set_format_osd(1 << 30)
dev.submit_and_wait(command)
print "status", command.status

pid = 0x10000

# create paritition
print "create partition"
command = pyosd.OSDCommand()
command.set_create_partition(pid)
dev.submit_and_wait(command)
print "status", command.status
if command.status:
	print command.show_sense(),

oid = 0x10010

# create particular oid
print "create particular object"
command = pyosd.OSDCommand()
command.set_create(pid, oid)
dev.submit_and_wait(command)
print "status", command.status
if command.status:
	print command.show_sense(),

ccap_page = 0xfffffffe
ccap_number_oid = 4

uiap_page = 0x0
uiap_number_len = 0x82

# create any oid, ask for result
print "create any object"
command = pyosd.OSDCommand()
command.set_create(pid)
attr = pyosd.OSDAttr(pyosd.ATTR_GET, ccap_page, ccap_number_oid, 8)
command.attr_build(attr)
dev.submit_and_wait(command)
print "status", command.status
if command.status:
	print command.show_sense(),
else:
	command.attr_resolve(attr)
	print "created oid", pyosd.ntohll(attr.val)

# two attrs
print "getattr"
command = pyosd.OSDCommand()
command.set_get_attributes(pid, oid)
attr = [ pyosd.OSDAttr(pyosd.ATTR_GET, ccap_page, ccap_number_oid, 8), \
         pyosd.OSDAttr(pyosd.ATTR_GET, uiap_page, uiap_number_len, 8) ]
command.attr_build(attr)
dev.submit_and_wait(command)
print "status", command.status
if command.status:
	print command.show_sense(),
else:
	command.attr_resolve(attr)
	verify_oid = pyosd.ntohll(attr[0].val)
	len = pyosd.ntohll(attr[1].val)
	print "verify oid", oid, "has len", len

dev.close()

