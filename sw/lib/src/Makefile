include $(TOP_DIR)/prefix.make
obj := $(patsubst %.cpp, $(TOP_DIR)/lib/$(BUILD_ID)/%.o, $(wildcard *.cpp))
LIB := $(TOP_DIR)/lib/$(BUILD_ID)

INCLUDES := -I../../rorcfs/include -I../$(FIRMWARE_BASE)
ifdef INSTALL_HOME
	INCLUDES += -I$(INSTALL_HOME)/include
endif

FLI=
ifeq ($(BUILD_ID),sim-debug)
FLI=$(TOP_DIR)/lib/fli.so
endif
ifeq ($(BUILD_ID),sim)
FLI=$(TOP_DIR)/lib/fli.so
endif


CFLAGS += -c -fPIC $(INCLUDES) -lpthread
LD=g++ -shared -Bsymbolic -o

all : $(TOP_DIR)/lib/$(BUILD_ID)/librorc.so $(FLI)

$(obj) : $(TOP_DIR)/lib/$(BUILD_ID)/%.o : %.cpp %.hh ../$(FIRMWARE_BASE)/rorc_registers.h
	$(CXX) $(CFLAGS) -o $@ $< 

$(TOP_DIR)/lib/$(BUILD_ID)/librorc.a: $(obj)	
	$(AR) -rcs $@ $^

$(TOP_DIR)/lib/$(BUILD_ID)/librorc.so: $(obj)
	$(LD) $@ $^



$(TOP_DIR)/lib/$(BUILD_ID)/flisock.o: flisock.c
	gcc -c -fPIC $(CFLAGS) $< -o $@

$(TOP_DIR)/lib/fli.so: $(TOP_DIR)/lib/$(BUILD_ID)/flisock.o
	gcc -shared -Bsymbolic -fPIC -L$(LIB) $< -o $@

clean:

