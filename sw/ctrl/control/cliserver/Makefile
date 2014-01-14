# $Id: Makefile 19 2013-08-10 14:51:50Z mueller $
#
# Copyright 2013- by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
#
# Compile and Link search paths
#
INCLFLAGS  = -I${CBMNETPATH} -I${SIMPATH}/include
LDLIBS     = -L${CBMNETPATH}/lib -lcbmnetutil -lcbmnetcntlserver
LDLIBS    += -L${SIMPATH}/lib -lzmq -lboost_thread -lboost_system
#
# Object files to be included
#
OBJ_all    = cliserver.o
#
DEP_all    = $(OBJ_all:.o=.dep)
#
cliserver : $(OBJ_all)

#- generic part ----------------------------------------------------------------
#
include ../../make/generic_cpp.mk
include ../../make/generic_dep.mk
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
#
