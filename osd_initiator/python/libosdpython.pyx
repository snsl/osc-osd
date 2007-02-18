#
# Wrapper classes around osd initiator library.
#
cimport libosdpython

cdef class OSDDevice:
	cdef int handle

	def __init__(self, path):
		self.handle = dev_osd_open(path)
		if self.handle < 0:
			print "No open!"
			raise Exception 
	
	def __del__(self):
		if self.handle >= 0:
			dev_osd_close(self.handle)
			self.handle = -1
		else:
			print "Device not open."
			raise Exception 

	def get_handle(self):
		return self.handle

	def submit(self, cdb, cdb_len, outdata, outlen, inlen_alloc):
		cdef void *nodata
		cdef object pbuf
		cdef uint8_t *buf

		nodata = NULL
		pbuf = cdb.get_buffer()
		buf = <uint8_t*>pbuf
		osd_sgio_submit_and_wait_python(self.handle, buf,
		                                cdb_len, nodata, outlen,
						inlen_alloc)
		# XXX: get result tuple

	def WaitResponse(self):
		if self.handle < 0:
			raise Exception 
		#cdef uint64_t key 
		#dev_osd_wait_response(self.handle, &key)
		return key

	def WriteNoData(self, cdb):
		if self.handle <= 0:
			raise Exception 
		#dev_osd_write_nodata( 
	
	def Serial(self):
		if self.handle < 0:
			raise Exception 
		cdef char* ret 
		ret = osd_get_drive_serial(self.handle)
		if ret == NULL:
			return None 
		return ret

cdef class Command:
	cdef object osd, cdb
	cdef int cdb_len
	cdef object outdata
	cdef size_t inlen_alloc
	cdef size_t outlen

	def __init__(self, osd, cdb, cdb_len, inlen=0, outlen=0, outdata=0):
		self.osd = osd
		self.cdb = cdb
		self.cdb_len = cdb_len
		self.inlen_alloc = inlen
		self.outlen = outlen
		self.outdata = None
	def submit(self):
		(self.indata, self.inlen, self.status, self.sense, \
			self.sense_len) = \
		self.osd.submit(self.cdb, self.cdb_len, self.outdata, self.outlen, \
			self.inlen_alloc)


cdef class DriveDescription:
	cdef osd_drive_description *drives
	cdef int num_drives

	def __init__(self):
		osd_get_drive_list(&self.drives, &self.num_drives)

	def __del__(self):
		osd_free_drive_list(self.drives, self.num_drives)

	def __len__(self):
		return self.num_drives

	def __getitem__(self, key):
		if type(key) != int:
			raise KeyError
		if key < 0 or key >= self.num_drives:
			raise IndexError
		# XXX: how to look at drives[1], e.g.?
		return ( self.drives.targetname, self.drives.chardev )


cdef class CDB:
	cdef uint8_t buffer[256]

	def __init__(self):
		pass;

	def get_buffer(self):
		return buffer;

	def GetBufferDump(self):
		buf = ''
		for i from 0 <= i < 32:
			buf = buf + "%02x:" % (i*8)
			for j from 0 <= j < 8:
				buf = buf + " %02x" % (self.buffer[i*8+j])
			buf = buf + "\n"
		return buf

	#########################################
	## CDB command section
	## Warning: Code may be exceedingly dull
	#########################################

	def Append (self, pid, oid, len):
		set_cdb_osd_append(self.buffer, pid, oid, len)
		return self

	def Create (self, pid, requested_oid, num):
		set_cdb_osd_create(self.buffer, pid, requested_oid, num)
		return self

	def CreateAndWrite (self, pid, requested_oid, len, offset):
		set_cdb_osd_create_and_write(self.buffer, pid, requested_oid, len, offset)
		return self

	def CreateCollection (self, pid, requested_cid):
		set_cdb_osd_create_collection(self.buffer, pid, requested_cid)
		return self

	def CreatePartition (self, requested_pid):
		set_cdb_osd_create_partition(self.buffer, requested_pid)
		return self

	def Flush (self, pid, oid, flush_scope):
		set_cdb_osd_flush(self.buffer, pid, oid, flush_scope)
		return self

	def FlushCollection (self, pid, cid, flush_scope):
		set_cdb_osd_flush_collection(self.buffer, pid, cid, flush_scope)
		return self

	def FlushOSD (self, flush_scope):
		set_cdb_osd_flush_osd(self.buffer, flush_scope)
		return self

	def FlushPartition (self, pid, flush_scope):
		set_cdb_osd_flush_partition(self.buffer, pid, flush_scope)
		return self

	def FormatOSD (self, capacity):
		set_cdb_osd_format_osd(self.buffer, capacity)
		return self

	def GetAttributes (self, pid, oid):
		set_cdb_osd_get_attributes(self.buffer, pid, oid)
		return self

	def GetMemberAttributes (self, pid, cid):
		set_cdb_osd_get_member_attributes(self.buffer, pid, cid)
		return self

	def List (self, pid, list_id, alloc_len, initial_oid):
		set_cdb_osd_list(self.buffer, pid, list_id, alloc_len, initial_oid)
		return self

	def ListCollection (self, pid, cid, list_id, alloc_len, initial_oid):
		set_cdb_osd_list_collection(self.buffer, pid, cid, list_id, alloc_len, initial_oid)
		return self

	def PerformSCSICommand (self):
		set_cdb_osd_perform_scsi_command(self.buffer)
		return self

	def PerformTaskMgmtFunction (self):
		set_cdb_osd_perform_task_mgmt_func(self.buffer)
		return self

	def Query (self, pid, cid, query_len, alloc_len):
		set_cdb_osd_query(self.buffer, pid, cid, query_len, alloc_len)
		return self

	def Read (self, pid, oid, len, offset):
		set_cdb_osd_read(self.buffer, pid, oid, len, offset)
		return self

	def Remove (self, pid, oid):
		set_cdb_osd_remove(self.buffer, pid, oid)
		return self

	def RemoveCollection (self, pid, cid):
		set_cdb_osd_remove_collection(self.buffer, pid, cid)
		return self

	def RemoveMemberObjects (self, pid, cid):
		set_cdb_osd_remove_member_objects(self.buffer, pid, cid)
		return self

	def RemovePartition (self, pid):
		set_cdb_osd_remove_partition(self.buffer, pid)
		return self

	def SetAttributes(self, pid, oid):
		set_cdb_osd_set_attributes(self.buffer, pid, oid)
		return self

	def SetKey (self, key_to_set, pid, key, seed):
		# FIXME: Not sure how to Pyrexify this yet
		#set_cdb_osd_set_key(self.buffer, key_to_set, pid, key, seed[20])
		return nil 

	def SetMasterKey (self, dh_step, key, param_len, alloc_len):
		set_cdb_osd_set_master_key(self.buffer, dh_step, key, param_len, alloc_len)
		return self

	def SetMemberAttributes (self, pid, cid):
		set_cdb_osd_set_member_attributes(self.buffer, pid, cid)
		return self

	def Write (self, pid, oid, len, offset):
		set_cdb_osd_write(self.buffer, pid, oid, len, offset)
		return self
