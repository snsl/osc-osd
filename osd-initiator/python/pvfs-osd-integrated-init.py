#!/usr/bin/python
#
# Initialize the OSD side of a combined PVFS + OSD integrated server.
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
from pyosd import *

# submit, check status, print sense, or resolve attributes, returning them
def run(command):
	dev.submit_and_wait(command)
	if command.status != 0:
		print "Command failed:", command.show_sense(),
		assert 0 == 1
	return command.attr_resolve()

def usage():
	print >>sys.stderr, "Usage:", sys.argv[0], "<target name> <pid>"
	sys.exit(1)

if len(sys.argv) != 3:
	usage()
set_progname(sys.argv[0])
devname = sys.argv[1]
pid = int(sys.argv[2])

drives = OSDDriveList()
dev = False
for d in drives:
	if d.targetname == devname:
		dev = OSDDevice(d.chardev)
		break
if not dev:
	print >>sys.stderr, "Drive", devname, "not found."
	sys.exit(1)

# format and build the one partition
run(OSDCommand().set_format_osd(1<<30))
run(OSDCommand().set_create_partition(pid))

