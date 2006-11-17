SUBDIRS = kernel libosd tests

pcdata_DATA = libosd.pc
pcdatadir = $(libdir)/pkgconfig

EXTRA_DIST = \
	AUTHORS NEWS INSTALL COPYING README \
	ChangeLog \
	autogen.sh \
	configure.in \
	libosd.pc.in

sane_tags:
	ctags -R .

sane_tags_clean:
	rm -f tags

all:  	sane_tags

clean:  sane_tags_clean clean-recursive
