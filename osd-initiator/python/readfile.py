#!/usr/bin/python
#
# Given a pid,oid from the command line, read the file.  Optionally specify
# a target name, else it chooses the first (or only) one that is connected.
#
# Copyright (C) 2008 Pete Wyckoff <pw@osc.edu>
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
import optparse
from pyosd import *

# submit, check status, print sense, or resolve attributes, returning them
def run(command):
	dev.submit_and_wait(command)
	if command.status != 0:
		print "Command failed:", command.show_sense(),
		assert 0 == 1
	return command.attr_resolve()

def compare(buf1, buf2):
	for i in range(len(buf1)):
		if buf1[i] != buf2[i]:
			print >>sys.stderr, \
			    "Bufs differ at offset %d: %02x vs %02x" \
			    % (i, ord(buf1[i]), ord(buf2[i]))
			sys.exit(1)

def readfile(pid, oid):
	len = 1 << 10  # 1 kB chunk size
	pos = 0
	while True:
		command = OSDCommand()
		command.set_read(pid, oid, len, pos)
		dev.submit_and_wait(command)
		# read past end of object, okay
		if command.status == 2 and command.sense_key == 1 \
		   and command.sense_code == 0x3b17:
			sys.stdout.write(command.indata)
			break
		if command.status != 0:
			print "Command failed:", command.show_sense(),
			assert 0 == 1
		sys.stdout.write(command.indata)
		pos += command.inlen

def writefile(pid, oid):
	len = 1 << 20  # 1 MB chunk size
	pos = 0
	while True:
		buf = sys.stdin.read(len)
		if buf == "":
			break
		command = OSDCommand()
		command.set_write(pid, oid, buf, pos)
		run(command)
		pos += len(buf)

parser = optparse.OptionParser()
parser.add_option("-t", "--target", dest="target",
		  help="use target with serial name TARGET", metavar="TARGET")
parser.add_option("-p", "--pid", dest="pid", help="pid of file")
parser.add_option("-o", "--oid", dest="oid", help="oid of file")
(options,args) = parser.parse_args()
if len(args) != 0:
	parser.error("unknown extra arguments")
	sys.exit(1)

if not options.pid:
	parser.error("pid argument is required")
	sys.exit(1)

if not options.oid:
	parser.error("oid argument is required")
	sys.exit(1)

pid = long(options.pid)
oid = long(options.oid)

set_progname(sys.argv[0])

random.seed()
drives = OSDDriveList()
if len(drives) < 1:
	print >>sys.stderr, "No drives."
	sys.exit(1)

if options.target:
	for d in drives:
		if d.targetname == options.target:
			chardev = d.chardev
			break
	if not chardev:
		print >>sys.stderr, "Drive", options.target, "not found."
		sys.exit(1)
else:
	chardev = drives[0].chardev
	if len(drives) > 1:
		priint >>sys.stderr, "More than one drive, choosing first."

dev = OSDDevice(chardev)
if get_progname() == "readfile.py":
	readfile(pid, oid)
elif get_progname() == "writefile.py":
	writefile(pid, oid)
else:
	print >>sys.stderr, \
	      "Expecting to be called as readfile.py or writefile.py."
	sys.exit(1)

