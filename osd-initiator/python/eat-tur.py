#!/usr/bin/python
#
# Recent STGT generates a unit attenion for poweron/reset for each
# new initiator that connects.  This gets in the way of PVFS, so eat
# it by sending a TUR.
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

def usage():
	print >>sys.stderr, "Usage:", sys.argv[0], "<target name>"
	sys.exit(1)

set_progname(sys.argv[0])
if len(sys.argv) == 1:
	usage()

def tur(dev, devname):
	# eat poweron attention with a tur
	command = OSDCommand().set_test_unit_ready()
	dev.submit_and_wait(command)
	if command.status == 0:
		print "Drive", devname, "okay"
		return  # no attention, command completed okay
	if (command.status == 2 and \
	    command.sense_key == OSD_SSK_UNIT_ATTENTION and \
	    command.sense_code == OSD_ASC_POWER_ON_OCCURRED):
		return
	print >>sys.stderr, "Drive", devname, "command failed:", \
			    command.show_sense()


drives = OSDDriveList()

for devname in sys.argv[1:]:
	dev = False
	for d in drives:
		if d.targetname == devname:
			dev = OSDDevice(d.chardev)
			break
	if not dev:
		print >>sys.stderr, sys.argv[0] + ": Drive", devname, \
				    "not found."
		continue

	tur(dev, devname)

