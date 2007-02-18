#!/usr/bin/python


import sys
from libosdpython import *
from command import *

drives = DriveDescription()
print "num_drives", len(drives)
print "drives[0]", drives[0]
(name, chardev) = drives[0]

osd = OSDDevice(chardev)
print "Serial is: '" + osd.Serial() + "'"

pid = 0x100004

cdb = CDB()
cdb.CreatePartition(pid)
print cdb.GetBufferDump(),

command = Command(osd, cdb, 200, outlen=80)
ret = command.submit()
print "result: status", command.status

