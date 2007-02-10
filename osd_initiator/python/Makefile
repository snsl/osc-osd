all: libosdpython.so

libosdpython.so: osd_wrap.o ../libosd.a ../../util/libosdutil.a
	gcc -shared -o $@ $^

osd_wrap.o: osd_wrap.c
	gcc -g -fPIC -c $< -I/usr/include/python2.4

osd_wrap.c: libosdpython.pyx libosdpython.pxd
	pyrexc -I../libosd -o $@ $<

libosdpython.pxd: libosdpython.pxd.in extract.py ../interface.h ../diskinfo.h
	python ./extract.py libosdpython.pxd.in > $@

clean:
	rm -f libosdpython.pxd osd_wrap.c osd_wrap.o libosdpython.so
