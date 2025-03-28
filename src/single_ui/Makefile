SHELL=/bin/bash
SRC_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
DELIVER_PATH=$(SRC_DIR)/build

all:dist

dist:$(DELIVER_PATH) $(SRC_DIR)/node_modules
	pushd $(SRC_DIR);npm run build

$(SRC_DIR)/node_modules:$(DELIVER_PATH)
	pushd $(SRC_DIR);npm install

$(DELIVER_PATH):
	[ -d $(DELIVER_PATH) ] || mkdir -p $(DELIVER_PATH)

clean:
	rm -rf $(DELIVER_PATH)

.PHONY:all dist $(DELIVER_PATH) clean