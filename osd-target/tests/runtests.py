#!/usr/bin/python -tu

import re
import os
import time
import sqlite
import commands
from results import results
from tests import *

class runtests:
	def __init__(self, dbname=None):
		self.__checkmakedef()
		if dbname == None:
			self.con = sqlite.connect("results.db")
		else: 
			self.con = sqlite.connect(dbname)
		self.con.check_same_thread = False
		self.con.enable_callback_tracebacks = True
		self.cur = self.con.cursor()
		self.cur.execute("PRAGMA auto_vacuum = 1;")
		self.gentestid = gentestid(self.cur, "gentestid")
		self.results = results(self.cur, "results", self.gentestid)
		ref = (self.gentestid, self.results)
		self.create = create(self.cur, "create_", ref, "create")
		self.setattr = setattr(self.cur, "setattr", ref, "setattr")
		self.getattr = getattr(self.cur, "getattr", ref, "getattr")
		self.list = getattr(self.cur, "list", ref, "list")
		self.listattr = listattr(self.cur, "listattr", ref, "list")
		self.query = query(self.cur, "query", ref, "query")
		self.sma = sma(self.cur, "sma", ref, "set_member_attributes")
		self.timecoll = timecoll(self.cur, "timecoll", ref, "time-coll")
		self.date = time.strftime('%F-%T')
		v = commands.getoutput('svn info | grep "^Rev" | cut -f2 -d\ ')
		self.version = int(v)

	def create_tests(self):
		self.gentestid.create()
		self.results.create()
		self.create.create()
		self.setattr.create()
		self.getattr.create()
		self.list.create()
		self.listattr.create()
		self.query.create()
		self.sma.create()
		self.timecoll.create()
		self.con.commit()

	def drop_tests(self):
		self.create.drop()
		self.setattr.drop()
		self.getattr.drop()
		self.list.drop()
		self.listattr.drop()
		self.query.drop()
		self.sma.drop()
		self.timecoll.drop()
		self.results.drop()
		self.gentestid.drop()
		self.con.commit()

	def populate_tests(self):
		self.con.isolation_level = "DEFERRED"
		self.create.populate()
		self.setattr.populate()
		self.getattr.populate()
		self.list.populate()
		self.listattr.populate()
		self.query.populate()
		self.sma.populate()
		self.timecoll.populate()
		self.con.commit()

	def populate_results(self):
		self.con.isolation_level = "DEFERRED"
		tests = [self.create, self.setattr, self.getattr, self.list,
				self.listattr, self.query, self.sma, self.timecoll]
		for t in tests:
			for tid, mu, sd, u in t.runall():
				self.results.insert(self.date, self.version, tid, mu, sd, 
						None, u)
		self.con.commit()

	def __checkmakedef(self, compile=False):
		try:
			make = '../../Makedefs'
			f = open(make)
			opt = re.compile("^OPT :=", re.IGNORECASE)
			dbg = re.compile(".*-g.*", re.IGNORECASE)
			copt = 0
			for l in f.readlines():
				if opt.search(l):
					assert not dbg.search(l), 'Incorrect compile opts: '+l
					copt = 1
			assert copt == 1, 'OPT not defined in ' + make
			f.close()
			if compile:
				assert os.system('make clean && make') == 0, 'Make failed'
		except IOError:
			print 'unknown file:' + make
			os._exit(1)


if __name__ == "__main__":
	rt = runtests()
#	rt.drop_tests()
#	rt.create_tests()
#	rt.populate_tests()
#	rt.populate_results()
	rt.timecoll.drop()
	rt.timecoll.create()
	rt.timecoll.populate()
	for tid, mu, sd, u in rt.timecoll.runall():
		rt.results.insert(rt.date, rt.version, tid, mu, sd, None, u)
	rt.con.commit()

