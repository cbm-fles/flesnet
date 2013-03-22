SUBDIRS := $(shell ls */Makefile | sed -r 's/\/.*//')
TOP_DIR := $(shell pwd)
FIRMWARE_BASE :=  ../../
include prefix.make

all: release

clean: subdirs-clean
	$(RM) -rf lib

release: BUILD_ID := release
release: subdirs

sim: CFLAGS += -DSIM
sim: BUILD_ID := sim
sim: subdirs

sim-debug: CFLAGS += -DSIM -DDEBUG -g -Wall -DFLI_DEBUG
#sim-debug: CFLAGS += -DSIM -DDEBUG -g -Wall
sim-debug: BUILD_ID := sim-debug
sim-debug: subdirs

debug: CFLAGS += -DDEBUG -g -Wall
debug: BUILD_ID := debug
debug: subdirs

subdirs:
	mkdir -p $(TOP_DIR)/lib/$(BUILD_ID)
	@for DIR in ${SUBDIRS}; do \
		cd $$DIR; \
		$(MAKE) all; \
		cd ..; \
	 done

subdirs-clean:
	@for DIR in $(SUBDIRS); do \
		cd $$DIR; \
		$(MAKE) clean; \
		cd ..; \
	done

.PHONY: all subdirs subdirs-clean debug sim-debug sim release
