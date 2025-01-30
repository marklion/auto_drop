SHELL=/bin/bash
SRC_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
SRC_DIR:=$(SRC_DIR)/src
DELIVER_PATH=$(SRC_DIR)/../build
SUB_DIR=sm public core
BUILD_MODE=build
OUTBOUND_DELIVER_PATH=$(DELIVER_PATH)
export BUILD_MODE
export OUTBOUND_DELIVER_PATH

.SILENT:
pack:all
	tar zcf ad_deliver.tar.gz -C $(DELIVER_PATH) bin lib
	cat $(SRC_DIR)/../deploy.sh ad_deliver.tar.gz > $(DELIVER_PATH)/install.sh
	chmod +x $(DELIVER_PATH)/install.sh
	rm ad_deliver.tar.gz

all:$(DELIVER_PATH)
$(DELIVER_PATH):$(SUB_DIR)
	[ -d $@ ] || mkdir $@
	for component in $^;do [ -d $(SRC_DIR)/$$component/build ] && cp -a $(SRC_DIR)/$$component/build/* $@/ || echo no_assert; done

$(SUB_DIR):
	$(MAKE) -C $(SRC_DIR)/$@

sm:public
core:sm

clean:
	rm -rf $(DELIVER_PATH)
	for sub_component in $(SUB_DIR); do make clean -C $(SRC_DIR)/$$sub_component;done

.PHONY:all $(SUB_DIR) $(DELIVER_PATH) clean pack