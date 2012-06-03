#!/usr/bin/python -tu
#
# Generic statistics.
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

import random
import math

random.seed()

class stats:
    def mean(self, l):
        return sum(l)*1.0/len(l)

    def var(self, l):
        mu = self.mean(l)
        N = len(l)
        var = 0.0
        for i in l:
            var += i**2
        var -= N*(mu**2)
        return var/(N - 1)

    def stdev(self, l):
        return math.sqrt(self.var(l))

    def median(self, li):
        l = li[:]
        N = len(l)
        if N == 0:
            return
        if N % 2 == 1:
            median = (N+1)/2 - 1
        else:
            median = N/2
        return self.select(l, median, 0, N-1) 

    def partition(self, l, left, right, pi):
        pv = l[pi]
        l[right], l[pi] = l[pi], l[right]
        ind = left
        for i in range(left, right+1):
            if l[i] < pv:
                l[ind], l[i] = l[i], l[ind]
                ind += 1
        l[right], l[ind] = l[ind], l[right]
        return ind

    def select(self, l, k, left, right):
        while 1:
            pi = random.randint(left, right)
            ind = self.partition(l, left, right, pi)
            if ind == k:
                return l[k]
            elif ind < k:
                left = ind + 1
            else:
                right = ind - 1
    def latest(self, l):
        return l[len(l) - 1]


if __name__ == "__main__":
    s = stats()
    print s.median([1])
    print s.median([])
    print s.median([10, 11, 14, 9, 2, 5, 7, 23])
    print s.median([5, 3, 4, 2,2, 2,2])
