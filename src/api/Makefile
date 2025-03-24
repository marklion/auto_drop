SHELL=/bin/bash
SRC_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
DELIVER_PATH=$(SRC_DIR)/build

all:dist

dist:$(DELIVER_PATH) $(SRC_DIR)/node_modules $(SRC_DIR)/src/index.js
	pushd $(SRC_DIR);npm run build

$(SRC_DIR)/node_modules:
	pushd $(SRC_DIR);npm install

$(DELIVER_PATH):
	[ -d $(DELIVER_PATH) ] || mkdir -p $(DELIVER_PATH)

.PHONY:all dist $(DELIVER_PATH)