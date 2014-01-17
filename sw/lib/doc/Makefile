include $(TOP_DIR)/prefix.make

ifeq ($(BUILD_ID),sim-debug)
	OBJ = 
else
	OBJ = librorc/index.html
endif

DEPS := $(wildcard $(TOP_DIR)/src/*.cpp)
DEPS += $(wildcard $(TOP_DIR)/src/*.hh)

all: $(OBJ)

$(OBJ): $(DEPS)
	doxygen librorc.config

clean:
	$(RM) -rf librorc
