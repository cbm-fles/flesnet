# $Id: Makefile 19 2013-08-10 14:51:50Z mueller $
#
# Copyright 2013- by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
#
# Name of the sharable library
#
SONAME     = cbmnetcntlserver
SOMAJV     = 1
SOMINV     = 0
SOPATH     = ../../lib
#
# Compile and Link search paths
#
INCLFLAGS  = -I${CBMNETPATH} -I${SIMPATH}/include
LDLIBS     = -L${CBMNETPATH}/lib -lcbmnetutil
LDLIBS    += -L${SIMPATH}/lib -lzmq
#
# Object files to be included
#
OBJ_all    = ControlServer.o ServSeqItem.o
#
DEP_all    = $(OBJ_all:.o=.dep)
#
#- generic part ----------------------------------------------------------------
#
SOFILE     = lib$(SONAME).so
SOFILEV    = lib$(SONAME).so.$(SOMAJV)
SOFILEVV   = lib$(SONAME).so.$(SOMAJV).$(SOMINV)
#
include ../../make/generic_cpp.mk
include ../../make/generic_dep.mk
include ../../make/generic_so.mk
include ../../make/dontincdep.mk
#
# The magic auto-dependency include
#
ifndef DONTINCDEP
include $(DEP_all)
endif
#
# cleanup phonies:
#
.PHONY    : clean cleandep distclean
clean     :
	@ rm -f $(OBJ_all)
	@ echo "Object files removed"
#
cleandep  :
	@ rm -f $(DEP_all)
	@ echo "Dependency files removed"
#
distclean : clean cleandep
	@ rm -f $(SOPATH)/lib$(SONAME).a $(SOPATH)/lib$(SONAME).so*
	@ echo "Libraries removed"
#
