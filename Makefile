# Comment/uncomment the following line to disable/enable debugging
DEBUG = y

# Add debugging flags to the modern Kbuild variable
ifeq ($(DEBUG),y)
  # -g adds debug symbols (useful if you ever use a kernel debugger).
  # -DDEBUG tells the compiler to activate all pr_debug() macros in your C code.
  ccflags-y += -g -DDEBUG
endif

# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)
	obj-m := fritz_module.o

# Otherwise we were called directly from the command
# line; invoke the kernel build system.
else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

endif
