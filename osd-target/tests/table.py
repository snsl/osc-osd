#!/usr/bin/python -tu

#$Id$

import re
import os
import sqlite
import commands

class table:
	def __init__(self, cur, name, ref=None):
		assert cur != None, ("Invalid value None for cur")
		assert name != None, ("Invalid value None for name")
		self.cur = cur
		self.name = name
		self.ref = ref #ref: tables on which this one depends

	def create(self):
		raise NotImplementedError

	def drop(self):
		q = "DROP TABLE IF EXISTS %s;" % self.name
		self.cur.execute(q)

	def insert(self):
		raise NotImplementedError

	def delete(self):
		raise NotImplementedError

	def getall(self):
		q = " SELECT * FROM %s;" % self.name
		self.cur.execute(q)
		return self.cur.fetchall()

