# $Id: generic_dep.mk 3 2013-07-06 12:30:39Z mueller $
#
#  Revision History: 
# Date         Rev Version  Comment
# 2011-01-09   354   1.0    Initial version (from wrepo/make/generic_dep.mk)
#---
#
# Dependency generation rules
#
%.dep: %.c
	@ echo "$(CC) -MM  $< | sed ... > $@"
	@ $(SHELL) -ec '$(CC) -MM $(CPPFLAGS) $(CFLAGS) $< \
		| sed '\''s/\($*\.o\)[ :]*/\1 $@ : /g'\'' > $@'
%.dep: %.cpp
	@ echo "$(CXX) -MM  $< | sed ... > $@"
	@ $(SHELL) -ec '$(CXX) -MM $(CPPFLAGS) $(CXXFLAGS) $< \
		| sed '\''s/\($*\.o\)[ :]*/\1 $@ : /g'\'' > $@'
#
