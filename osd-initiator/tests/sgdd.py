#!/usr/bin/python -tu

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


