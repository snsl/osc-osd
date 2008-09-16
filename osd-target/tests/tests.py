#!/usr/bin/python -tu

#$Id$

import re
import os
import sqlite
import commands
from table import table

class gentestid(table):
	def __init__(self, cur, name):
		table.__init__(self, cur, name, None)

	def create(self):
		q = """
			CREATE TABLE IF NOT EXISTS %s (
				testid INTEGER NOT NULL,
				PRIMARY KEY (testid)
			); """ % self.name
		self.cur.execute(q)

	def insert(self):
		q = "INSERT INTO %s VALUES (NULL);" % self.name
		self.cur.execute(q)

	def delete(self, testid):
		q = "DELETE FROM %s WHERE testid = %d;" %s (self.name, testid)
		self.cur.execute(q)

	def deletemax(self):
		q = "DELETE FROM %s WHERE testid = (SELECT MAX(testid) FROM %s);" % \
				(self.name, self.name)
		self.cur.execute(q)


class test(table):
	def __init__(self, cur, name, ref, cmd):
		table.__init__(self, cur, name, ref)
		self.cmd = cmd
		self.gentestid = self.ref[0]
		self.results = self.ref[1]

	def populate(self):
		raise NotImplementedError

	def runall(self):
		raise NotImplementedError

	def drop(self):
		q = """
			DELETE FROM %s WHERE testid IN (SELECT testid FROM %s);
			DELETE FROM %s WHERE testid IN (SELECT testid FROM %s);
			DROP TABLE IF EXISTS %s;
			""" % (self.results.name, self.name, self.gentestid.name,
					self.name, self.name)
		try:
			self.cur.execute(q)
		except sqlite.DatabaseError:
			pass

	def inserttest(self, q, err):
		self.gentestid.insert()
		try:
			self.cur.execute(q)
		except sqlite.IntegrityError:
			print err
			self.gentestid.deletemax()

	def deletetest(self, q):
		self.cur.execute(q)
		res = self.cur.fetchall()
		if res == None:
			return
		testid = res[0][0]
		q = "DELETE FROM %s WHERE testid = %d;" % (self.name, testid)
		self.cur.execute(q)
		self.gentestid.delete(testid)


class mdtest(test):
	def __init__(self, cur, name, ref, cmd):
		test.__init__(self, cur, name, ref, cmd)

	def create(self):
		q = """
			CREATE TABLE IF NOT EXISTS %s (
				testid INTEGER NOT NULL,
				numobj INTEGER NOT NULL,
				numiter INTEGER NOT NULL,
				desc TEXT,
				PRIMARY KEY (testid),
				UNIQUE (numobj, numiter),
				FOREIGN KEY (testid) REFERENCES %s (testid)
			); """ % (self.name, self.gentestid.name)
		self.cur.execute(q)

	def insert(self, numobj, numiter, desc='NULL'):
		q = "INSERT INTO %s SELECT MAX(testid), %d, %d, %s FROM %s;" % \
				(self.name, numobj, numiter, desc, self.gentestid.name)
		err = 'test case %d %d already present' % (numobj, numiter)
		self.inserttest(str(q), str(err))
	
	def delete(self, numobj, numiter):
		q = "SELECT testid FROM %s WHERE numobj = %d AND numiter = %d;" % \
				(self.name, numobj, numiter);
		self.deletetest(str(q))
		
	def populate(self):
		for no in range(3, 17):
			self.insert(1 << no, 10)
		
	def runall(self):
		results = []
		sp = re.compile('\s+')
		for tid, no, ni, x in self.getall():
			cmd = './'+ self.cmd +' -o '+ str(no) +' -i '+ str(ni)
			o = commands.getoutput(cmd)
			print o
			t = sp.split(o)
			results.append((tid, float(t[6]), float(t[8]), t[9]))
		return results
		

class create(mdtest):
	def __init__(self, cur, name, ref, cmd):
		mdtest.__init__(self, cur, name, ref, cmd)


class setattr(mdtest):
	def __init__(self, cur, name, ref, cmd):
		mdtest.__init__(self, cur, name, ref, cmd)


class getattr(mdtest):
	def __init__(self, cur, name, ref, cmd):
		mdtest.__init__(self, cur, name, ref, cmd)


class list(mdtest):
	def __init__(self, cur, name, ref, cmd):
		mdtest.__init__(self, cur, name, ref, cmd)


class listattr(test):
	def __init__(self, cur, name, ref, cmd):
		test.__init__(self, cur, name, ref, cmd)
	
	def create(self):
		q = """ 
			CREATE TABLE IF NOT EXISTS %s (
				testid INTEGER NOT NULL,
				numobj INTEGER NOT NULL,
				numattr INTEGER NOT NULL,
				numret INTEGER NOT NULL,
				numiter INTEGER NOT NULL,
				desc TEXT,
				PRIMARY KEY (testid),
				UNIQUE (numobj, numattr, numret, numiter),
				FOREIGN KEY (testid) REFERENCES %s (testid)
			); """ % (self.name, self.gentestid.name)
		self.cur.execute(q)

	def insert(self, numobj, numattr, numret, numiter, desc='NULL'):
		q = "INSERT INTO %s SELECT MAX(testid), %d, %d, %d, %d, %s FROM %s;"\
				% (self.name, numobj, numattr, numret, numiter, desc, 
						self.gentestid.name)
		err = "test case %d %d %d %d exists!" % \
				(numobj, numattr, numret, numiter)
		self.inserttest(str(q), str(err))

	def delete(self, numobj, numattr, numret, numiter):
		q = """
			SELECT testid FROM %s WHERE numobj = %d AND numattr = %d AND
				numret = %d AND numiter = %d;
			""" % (self.name, numobj, numattr, numret, numiter);
		self.deletetest(str(q))

	def populate(self):
		# vary numobj
		for no in range(3, 14):
			self.insert(1 << no, 4, 1, 10)

		# vary numattr
		for na in range(1, 8):
			if (1 << na) == 4:
				continue
			self.insert(1024, 1 << na, 1, 10)

		# vary numret
		for nr in range(6):
			self.insert(256, 32, 1 << nr, 10)

	def runall(self):
		results = []
		sp = re.compile('\s+')
		for tid, no, na, nr, ni, x in self.getall():
			cmd = './%s -o %d -a %d -r %d -i %d' % (self.cmd, no, na, nr, ni)
			o = commands.getoutput(str(cmd))
			print o
			t = sp.split(o)
			results.append((tid, float(t[10]), float(t[12]), t[13]))
		return results


class query(test):
	def __init__(self, cur, name, ref, cmd):
		test.__init__(self, cur, name, ref, cmd)
	
	def create(self):
		q = """ 
			CREATE TABLE IF NOT EXISTS %s (
				testid INTEGER NOT NULL,
				numobj INTEGER NOT NULL,
				collsz INTEGER NOT NULL,
				numattr INTEGER NOT NULL,
				nummatch INTEGER NOT NULL,
				numqc INTEGER NOT NULL,
				numiter INTEGER NOT NULL,
				desc TEXT,
				PRIMARY KEY (testid),
				UNIQUE (numobj, collsz, nummatch, numattr, numqc, numiter),
				FOREIGN KEY (testid) REFERENCES %s (testid)
			); """ % (self.name, self.gentestid.name)
		self.cur.execute(q)

	def insert(self, numobj, collsz, numattr, nummatch, numqc, numiter, 
			desc='NULL'):
		q = """ INSERT INTO %s SELECT MAX(testid), %d, %d, %d, %d, %d, %d, %s 
				FROM %s;""" % (self.name, numobj, collsz, numattr, 
						nummatch, numqc, numiter, desc, self.gentestid.name)
		err = "test case %d, %d, %d, %d, %d, %d exists!" % (numobj, collsz,
				numattr, nummatch, numqc, numiter)
		self.inserttest(str(q), str(err))

	def delete(self, numobj, collsz, numattr, nummatch, numqc, numiter):
		q = """ SELECT testid FROM %s WHERE numobj = %d AND collsz = %d AND 
			numattr = %d AND  nummatch = %d AND numqc = %d AND numiter = %d;
			""" % (self.name, numobj, collsz, numattr, nummatch, numqc, 
					numiter)
		self.deletetest(str(q))

	def populate(self):
		# vary numobj
		for no in range(10, 15):
			self.insert(1 << no, 256, 4, 4, 2, 10)

		# vary collsz
		for cs in range(4, 11):
			self.insert(4096, 1 << cs, 4, 4, 1, 10)

		# vary numattr
		for na in range(1, 9):
			if (1 << na) == 4:
				continue
			self.insert(1024, 256, 1 << na, 4, 2, 10)

		# vary nummatch
		for nm in range(4, 10):
			self.insert(4096, 1024, 4, 1 << nm, 2, 10)

		# vary numqc
		for qc in range(1, 9):
			self.insert(1024, 512, 16, 4, qc, 10)
		
	def runall(self):
		results = []
		sp = re.compile('\s+')
		for tid, no, cs, na, nm, qc, ni, x in self.getall():
			cmd = './%s -o %d -c %d -a %d -m %d -n %d -i %d' % (self.cmd, 
					no, cs, na, nm, qc, ni)
			o = commands.getoutput(str(cmd))
			print o
			t = sp.split(o)
			results.append((tid, float(t[14]), float(t[16]), t[17]))
		return results


class sma(test):
	def __init__(self, cur, name, ref, cmd):
		test.__init__(self, cur, name, ref, cmd)
	
	def create(self):
		q = """ 
			CREATE TABLE IF NOT EXISTS %s (
				testid INTEGER NOT NULL,
				numobj INTEGER NOT NULL,
				collsz INTEGER NOT NULL,
				numattr INTEGER NOT NULL,
				numset INTEGER NOT NULL,
				numiter INTEGER NOT NULL,
				desc TEXT,
				PRIMARY KEY (testid),
				UNIQUE (numobj, collsz, numattr, numset, numiter),
				FOREIGN KEY (testid) REFERENCES %s (testid)
			); """ % (self.name, self.gentestid.name)
		self.cur.execute(q)

	def insert(self, numobj, collsz, numattr, numset, numiter, desc='NULL'):
		q = """ INSERT INTO %s SELECT MAX(testid), %d, %d, %d, %d, %d, %s 
				FROM %s;""" % (self.name, numobj, collsz, numattr, numset, 
						numiter, desc, self.gentestid.name)
		err = "test case %d, %d, %d, %d, %d exists!" % (numobj, collsz,
				numattr, numset, numiter)
		self.inserttest(str(q), str(err))

	def delete(self, numobj, collsz, numattr, numset, numiter):
		q = """ SELECT testid FROM %s WHERE numobj = %d AND collsz = %d AND 
			numattr = %d AND numset = %d AND numiter = %d; """ % (self.name, 
					numobj, collsz, numattr, numset, numiter)
		self.deletetest(str(q))

	def populate(self):
		# vary numobj
		for no in range(10, 15):
			self.insert(1 << no, 256, 4, 2, 10)

		# vary collsz
		for cs in range(4, 11):
			self.insert(4096, 1 << cs, 4, 1, 10)

		# vary numattr
		for na in range(1, 9):
			if (1 << na) == 4:
				continue
			self.insert(1024, 256, 1 << na, 2, 10)

		# vary numset
		for ns in range(6):
			self.insert(4096, 1024, 32, 1 << ns, 10)

	def runall(self):
		results = []
		sp = re.compile('\s+')
		for tid, no, cs, na, ns, ni, x in self.getall():
			cmd = './%s -o %d -c %d -a %d -s %d -i %d' % (self.cmd, 
					no, cs, na, ns, ni)
			o = commands.getoutput(str(cmd))
			print o
			t = sp.split(o)
			results.append((tid, float(t[12]), float(t[14]), t[15]))
		return results
