CTAGSFLAGS="--fields=fksS"
SUBDIRS = kernel libosd

sane_tags:
	ctags -R .

sane_tags_clean:
	rm -f tags

all:  	sane_tags

clean:  sane_tags_clean clean-recursive
