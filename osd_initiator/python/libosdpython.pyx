cimport libosdpython

cdef extern from "Python.h":
	void Py_XINCREF (object)
	void Py_XDECREF (object)
	object PyString_FromStringAndSize(char *, int)
	ctypedef void *PyGILState_STATE
	void PyErr_Clear()
	PyGILState_STATE PyGILState_Ensure()
	void PyGILState_Release(PyGILState_STATE)


class OSDDevice:

	def __init__(self):
		self.handle = 0

	def Open(self, path):
		self.handle = dev_osd_open(path)
		if(self.handle <= 0):
			print "No open!"
	
	def Close(self):
		if self.handle and self.handle > 0:
			dev_osd_close(self.handle)
			self.handle = 0
		else:
			print "Hey that's lame!"
	
	def Serial(self):
		if self.handle <= 0:
			raise Exception 
		cdef char* ret 
		ret = osd_get_drive_serial(self.handle)
		if ret == NULL:
			return None 
		return ret
