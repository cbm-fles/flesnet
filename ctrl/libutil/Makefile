# $Id: Makefile 22 2013-08-11 19:55:56Z mueller $
#
# Copyright 2013- by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
#
# Name of the sharable library
#
SONAME     = cbmnetutil
SOMAJV     = 1
SOMINV     = 0
SOPATH     = ../lib
#
# Compile and Link search paths
#
INCLFLAGS  = -I${CBMNETPATH} -I${SIMPATH}/include
LDLIBS     = -L${SIMPATH}/lib -lzmq -lboost_system
#
# Object files to be included
#
OBJ_all    = Util.o
OBJ_all   += Rbits.o RerrMsg.o ReventFd.o Rexception.o
OBJ_all   += RiosState.o RlogFile.o RlogFileDefault.o RlogMsg.o RosFill.o 
OBJ_all   += RosPrintBvi.o RosPrintfBase.o RosPrintfS.o
#
DEP_all    = $(OBJ_all:.o=.dep)
#
#- generic part ----------------------------------------------------------------
#
SOFILE     = lib$(SONAME).so
SOFILEV    = lib$(SONAME).so.$(SOMAJV)
SOFILEVV   = lib$(SONAME).so.$(SOMAJV).$(SOMINV)
#
include ../make/generic_cpp.mk
include ../make/generic_dep.mk
include ../make/generic_so.mk
include ../make/dontincdep.mk
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
