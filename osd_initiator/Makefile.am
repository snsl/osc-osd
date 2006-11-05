CFLAGS = "-I$(PWD)/libosd"

suo.ko:
	$(MAKE) SUBDIRS=$(PWD)/kernel -C $(KDIR) modules 

clean-suo.ko:
	$(MAKE) SUBDIRS=$(PWD)/kernel -C $(KDIR) clean

all:  suo.ko

clean: clean-suo.ko

