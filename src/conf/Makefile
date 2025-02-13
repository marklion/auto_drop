SHELL=/bin/bash
SRC_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
DELIVER_PATH=$(SRC_DIR)/build
DELIVER_CONF_PATH=$(DELIVER_PATH)/conf

all:$(DELIVER_PATH)

$(DELIVER_PATH):$(DELIVER_CONF_PATH)
	[ -d $(DELIVER_PATH) ] || mkdir -p $(DELIVER_PATH)

$(DELIVER_CONF_PATH):
	[ -d $(DELIVER_CONF_PATH) ] || mkdir -p $(DELIVER_CONF_PATH)
	cp $(SRC_DIR)/nginx.conf $(DELIVER_CONF_PATH)
clean:
	rm -rf $(DELIVER_PATH) $(SRC_DIR)/cmake_tmp_dir

.PHONY:all $(DELIVER_PATH) $(DELIVER_CONF_PATH) clean