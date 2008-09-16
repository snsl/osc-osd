#!/usr/bin/python -tu

import re
import os
import sqlite
from table import table 

class results(table):
	def __init__(self, cur, name, ref):
		table.__init__(self, cur, name, ref)

	def create(self):
		q = """
			CREATE TABLE IF NOT EXISTS %s (
				tid INTEGER NOT NULL,
				date TEXT NOT NULL,
				version INTEGER NOT NULL,
				testid INTEGER NOT NULL,
				mu FLOAT,
				sd FLOAT,
				median FLOAT,
				units TEXT,
				PRIMARY KEY (tid),
				FOREIGN KEY (testid) REFERENCES %s (testid)
			); """ % (self.name, self.ref.name)
		self.cur.execute(q)

	def insert(self, date, ver, testid, mu, sd, median=None, units='us'):
		if median == None:
			q = """ 
				INSERT INTO %s VALUES (NULL, '%s', %d, %d, %f, %f, NULL, '%s');
				""" % (self.name, date, ver, testid, mu, sd, units)
		else:
			q = """
				INSERT INTO %s VALUES (NULL, '%s', %d, %d, %f, %f, %f, '%s');
				""" % (self.name, date, ver, testid, mu, sd, median, units)
		self.cur.execute(q) 

	def delete(self, date, version, testid):
		q = """ DELETE FROM %s WHERE date = '%s' AND version = %d AND 
					testid = %d;""" % (self.name, date, version, testid)
		self.cur.execute(q) 

