lib_LTLIBRARIES = libosd.la
osdincdir = $(includedir)/libosd

osdinc_HEADERS = \
	interface.h

libosd_la_SOURCES = \
	interface.c 

libosd_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)

INCLUDES = \
	-I$(KDIR)/include \
	-Wall
	
noinst_HEADERS = \
	./osd_attr_defs.h 	\
	./osd_attr.h 		\
	./osd_attr_numbers.h 	\
	./osd_attr_pages.h 	\
	./osd_cdb_cmd.h 	\
	./osd_cdb_formats.h 	\
	./osd_cdb.h 		\
	./osd_formats.h 	\
	./osd_ops.h 		\
	./osd_ops_utils.h 	\
	./osd_req.h 		\
	./osd_req_imp.h 	\
	./osd_sense.h 		\
	./osd_test_cmd.h 	\
	./osd_trace.h 		\
	./osd_defs.h 		\
	./osd_util.h 		\
	./osd_version.h 	\
	./util.h 		\
	./osd_hdr.h
