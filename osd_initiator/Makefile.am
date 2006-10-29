CFLAGS = "-I($PWD)/libosd"

suo.ko:
	$(MAKE) SUBDIRS=$(PWD)/kernel -C $(KDIR) modules 

all:  suo.ko
