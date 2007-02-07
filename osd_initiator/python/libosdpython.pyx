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

	def WaitResponse(self):
		if self.handle <= 0:
			raise Exception 
		cdef uint64_t key 
		dev_osd_wait_response(self.handle, &key)
		return key
	
	def Serial(self):
		if self.handle <= 0:
			raise Exception 
		cdef char* ret 
		ret = osd_get_drive_serial(self.handle)
		if ret == NULL:
			return None 
		return ret

cdef class CDB:
	cdef public char buffer[256]

	def __init__(self):
		pass;

	#########################################
	## CDB command section
	## Warning: Code may be exceddingly dull
	#########################################

	def Append (self, pid, oid, len):
		set_cdb_osd_append(<uint8_t*>self.buffer, pid, oid, len)
		return self

	def Create (self, pid, requested_oid, num):
		set_cdb_osd_create(<uint8_t*>self.buffer, pid, requested_oid, num)
		return self

	def CreateAndWrite (self, pid, requested_oid, len, offset):
		set_cdb_osd_create_and_write(<uint8_t*>self.buffer, pid, requested_oid, len, offset)
		return self

	def CreateCollection (self, pid, requested_cid):
		set_cdb_osd_create_collection(<uint8_t*>self.buffer, pid, requested_cid)
		return self

	def CreatePartition (self, requested_pid):
		set_cdb_osd_create_partition(<uint8_t*>self.buffer, requested_pid)
		return self

	def Flush (self, pid, oid, flush_scope):
		set_cdb_osd_flush(<uint8_t*>self.buffer, pid, oid, flush_scope)
		return self

	def FlushCollection (self, pid, cid, flush_scope):
		set_cdb_osd_flush_collection(<uint8_t*>self.buffer, pid, cid, flush_scope)
		return self

	def FlushOSD (self, flush_scope):
		set_cdb_osd_flush_osd(<uint8_t*>self.buffer, flush_scope)
		return self

	def FlushPartition (self, pid, flush_scope):
		set_cdb_osd_flush_partition(<uint8_t*>self.buffer, pid, flush_scope)
		return self

	def FormatOSD (self, capacity):
		set_cdb_osd_format_osd(<uint8_t*>self.buffer, capacity)
		return self

	def GetAttributes (self, pid, oid):
		set_cdb_osd_get_attributes(<uint8_t*>self.buffer, pid, oid)
		return self

	def GetMemberAttributes (self, pid, cid):
		set_cdb_osd_get_member_attributes(<uint8_t*>self.buffer, pid, cid)
		return self

	def List (self, pid, list_id, alloc_len, initial_oid):
		set_cdb_osd_list(<uint8_t*>self.buffer, pid, list_id, alloc_len, initial_oid)
		return self

	def ListCollection (self, pid, cid, list_id, alloc_len, initial_oid):
		set_cdb_osd_list_collection(<uint8_t*>self.buffer, pid, cid, list_id, alloc_len, initial_oid)
		return self

	def PerformSCSICommand (cdb):
		set_cdb_osd_perform_scsi_command(<uint8_t*>self.buffer)
		return self

	def PerformTaskMgmtFunction (cdb):
		set_cdb_osd_perform_task_mgmt_func(<uint8_t*>self.buffer)
		return self

	def Query (self, pid, cid, query_len, alloc_len):
		set_cdb_osd_query(<uint8_t*>self.buffer, pid, cid, query_len, alloc_len)
		return self

	def Read (self, pid, oid, len, offset):
		set_cdb_osd_read(<uint8_t*>self.buffer, pid, oid, len, offset)
		return self

	def Remove (self, pid, oid):
		set_cdb_osd_remove(<uint8_t*>self.buffer, pid, oid)
		return self

	def RemoveCollection (self, pid, cid):
		set_cdb_osd_remove_collection(<uint8_t*>self.buffer, pid, cid)
		return self

	def RemoveMemberObjects (self, pid, cid):
		set_cdb_osd_remove_member_objects(<uint8_t*>self.buffer, pid, cid)
		return self

	def RemovePartition (self, pid):
		set_cdb_osd_remove_partition(<uint8_t*>self.buffer, pid)
		return self

	def SetAttributes(self, pid, oid):
		set_cdb_osd_set_attributes(<uint8_t*>self.buffer, pid, oid)
		return self

	def SetKey (self, key_to_set, pid, key, seed):
		# FIXME: Not sure how to Pyrexify this yet
		#set_cdb_osd_set_key(<uint8_t*>self.buffer, key_to_set, pid, key, seed[20])
		return nil 

	def SetMasterKey (self, dh_step, key, param_len, alloc_len):
		set_cdb_osd_set_master_key(<uint8_t*>self.buffer, dh_step, key, param_len, alloc_len)
		return self

	def SetMemberAttributes (self, pid, cid):
		set_cdb_osd_set_member_attributes(<uint8_t*>self.buffer, pid, cid)
		return self

	def Write (self, pid, oid, len, offset):
		set_cdb_osd_write(<uint8_t*>self.buffer, pid, oid, len, offset)
		return self
