#!/usr/bin/python


from libosdpython import *

d = OSDDevice()
d.Open('/dev/sua')
d.Serial()
d.Close()
