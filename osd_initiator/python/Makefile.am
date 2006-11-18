INCLUDES = $(PYTHON_INCLUDES) -I$(top_srcdir) 

lib_LTLIBRARIES = libosdpython.la

osdpythondir = $(pyexecdir)
libosdpython_la_LDFLAGS = -Xlinker -export-dynamic
libosdpython_la_LIBADD = $(top_builddir)/libosd/libosd.la
libosdpython_la_SOURCES = \
	osd_wrap.c 	\
	test.c

osd_wrap.c: osd.i
	$(SWIG) -python -o $@ $<
