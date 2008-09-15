#!/usr/bin/python -tu 
#
# Start/stop a bunch of OSD targets on nodes from PBS.
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

import os, sys, commands, pwd

nodes = []
osdnodes = []
compnodes = []
testdir = os.environ["TMPDIR"];
id = pwd.getpwuid(os.getuid())[0]

def usage():
    print >>sys.stderr, "Usage:", sys.argv[0], "<start|stop>"
    os._exit(1)

def allify(n):
    (fdto, fdfrom) = os.popen2("/home/ananth/bin/allify")
    for i in n:
        print >>fdto, i
    fdto.close()
    s = fdfrom.read()
    fdfrom.close()
    s = s[:-1]
    return s

def init():
    global nodes, osdnodes, compnodes
    ret = commands.getoutput("echo $PBS_NODEFILE")
    if ret != "":
        o = commands.getoutput("uniq $PBS_NODEFILE")
        nodes = o.split('\n')
        osdnodes = [nodes[-1]]
        compnodes = nodes[:-1]
    else:
        print "Run the program from PBS root node"
        os._exit(1)

def start():
    assert osdnodes != []
    os.system("all -p " + allify(osdnodes) + " "
            + "cd " + testdir + " \;"
            + "rm -rf /tmp/tgt-" + id + " \; "
            + "mkdir /tmp/tgt-" + id + " \; "
            + "~/osd/osd-target/tgtd -d 9 \< /dev/null \&\> tgtd.log")
    assert compnodes != []
    os.system("all -p " + allify(compnodes) + " "
	    + "sudo /home/pw/src/osd/test/initiator start " + " ".join(osdnodes))

def stop():
    assert osdnodes != []
    os.system("all -p " + allify(compnodes) + " "
            + "sudo /home/pw/src/osd/test/initiator stop; ")

    assert compnodes != []
    os.system("all -p " + allify(osdnodes) + " "
            + "cd " + testdir + " \; "
            + "killall -9 tgtd \; "
            + "rm -f tgtd.log")


init()
if len(sys.argv) != 2:
    usage()
if sys.argv[1] == "start":
    start()
elif sys.argv[1] == "stop":
    stop()
