obj-m := rorcfs.o
rorcfs-objs := src/base.o src/buffer.o src/sysfs.o
KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
DST := $(PWD)/build/$(shell uname -r)

EXTRA_CFLAGS += -O -Wall $(CFLAGS) -DDEBUG
#EXTRA_CFLAGS += -O -Wall $(CFLAGS)

all: module

$(DST)/Makefile: $(PWD)/Makefile
	mkdir -p $(DST) $(DST)/src $(DST)/include
	ln -sf $(PWD)/src/*.c $(DST)/src/
	ln -sf $(PWD)/src/*.h $(DST)/src/
	ln -sf $(PWD)/include/*.h $(DST)/include/
	ln -sf $< $@

module: $(DST)/Makefile
	$(MAKE) -C $(KERNELDIR) M=$(DST) modules

install: $(DST)/Makefile module
	$(MAKE) -C $(KERNELDIR) M=$(DST) modules_install


clean:
	rm -rf $(DST)

distclean:
	$(RM) -rf $(PWD)/build
	$(RM) -rf rorcfs-trunk.tgz

archive: rorcfs-trunk.tgz
rorcfs-trunk.tgz:
	tar czf $@ ./* --exclude .svn --exclude $@ --exclude build
