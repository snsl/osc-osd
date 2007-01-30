INCLUDES = $(PYTHON_INCLUDES) -I$(top_srcdir) 
lib_LTLIBRARIES = libosdpython.la

PYREX_FLAGS = -+ -I $(top_builddir)/libosd

PYREX_SOURCE_FILES = \
	libosd.pyx

PYREX_HEADER_FILES = \
	libosd.pxd

libosdpython_la_LDFLAGS = -Xlinker -export-dynamic 

libosdpython_la_LIBADD = $(top_builddir)/libosd/libosd.la

libosdpython_la_SOURCES = \
	osd_wrap.c

pyrex_header_includes = $(addsuffix .in, $(PYREX_HEADER_FILES))

SUFFIXES = .pyd.in .pyd

%.pxd: %.pxd.in
	$(PYTHON) extract.py $< > .tmp_pyd
	mv .tmp_pyd $@

osd_wrap.c: $(PYREX_SOURCE_FILES) $(PYREX_HEADER_FILES)
	$(PYREX) $(PYREX_FLAGS) -o $@ $(PYREX_SOURCE_FILES)

EXTRA_DIST = \
	$(pyrex_header_includes)
