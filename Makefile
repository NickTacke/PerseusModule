obj-m += PerseusMod.o

# Kernel build directory
KDIR := /lib/modules/$(shell uname -r)/build

# Module build directory
BUILD_DIR := $(PWD)/build

# Ensure the build directory exists
_dummy := $(shell mkdir -p $(BUILD_DIR))

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	mv $(PWD)/*.o $(PWD)/.*.cmd $(PWD)/*.mod.c $(PWD)/*.mod $(PWD)/*.order $(PWD)/*.symvers $(BUILD_DIR)/
	@clear
	@echo "-[ Successfuly built kernel module ]-"
	@echo "-[ Use \"make clean\" to delete built files ]-"

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -rf $(BUILD_DIR)
