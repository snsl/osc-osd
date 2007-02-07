#!/usr/bin/python


from libosdpython import *

d = OSDDevice()
d.Open('/dev/sua')
print "Serial is: '" + d.Serial() + "'"
d.Close()
