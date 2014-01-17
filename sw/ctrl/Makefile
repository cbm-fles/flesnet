# $Id: Makefile 15 2013-08-06 16:20:18Z mueller $

DIRS  = libutil
DIRS += control

#
BUILDDIRS = $(DIRS:%=build-%)
CLEANDIRS = $(DIRS:%=clean-%)
DISTCDIRS = $(DIRS:%=distc-%)
#
.PHONY: all clean distclean
.PHONY: $(DIRS)
.PHONY: $(BUILDDIRS)
.PHONY: $(CLEANDIRS)
.PHONY: $(DISTCDIRS)
#
# build 'all' rule
#
all: $(BUILDDIRS)
#
build-control       : build-libutil
build-usbdaq        : build-control
#
$(BUILDDIRS):
	$(MAKE) -C $(@:build-%=%)
#
# clean rule
#
clean: $(CLEANDIRS)
$(CLEANDIRS): 
	$(MAKE) -C $(@:clean-%=%) clean
#
# distclean rule
#
distclean: $(DISTCDIRS)
$(DISTCDIRS): 
	$(MAKE) -C $(@:distc-%=%) distclean

