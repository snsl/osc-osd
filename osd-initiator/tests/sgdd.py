#!/usr/bin/python -tu
#
# Run sg_dd throughput timings.
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

import re, sys
import commands
from stats import stats

st = stats()

time = re.compile("^time", re.IGNORECASE)
sp = re.compile("\s+")

# iflag=direct if=/dev/sdb of=/tmp/z
def run(exp):
    if exp == "read":
        var = "if=/dev/sdb of=/dev/null"
    elif exp == "write":
        var = "of=/dev/sdb if=/dev/zero"

    for bs in range(4, 512, 4):
        r = []
        for j in range(100):
            s = "sg_dd blk_sgio=1 " + var +" bs=" + str(bs) + "K bpt=1 " + \
                    "count=1 time=1 2>&1"
            o = commands.getoutput(str(s))
            assert bool(time.match(o))
            w = sp.split(o)
            t = float(w[4])
            r.append(bs*1024/(t*1e6))
        print bs, st.mean(r), "+-", st.stdev(r)
    

if __name__ == "__main__":
    run("write")
    print "\n\n"
    run("read")


