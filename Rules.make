#
# This file contains rules which are shared between multiple Makefiles.
#
# It's a stripped and modified version of /usr/src/linux/Rules.make. [--jk]
#

#
# False targets.
#
.PHONY: dummy

#
# Get things started.
#
first_rule: modules

#
# ALSA hacks for extra code
#

subdir-y	+= $(extra-subdir-y)
subdir-m	+= $(extra-subdir-m)
subdir-n	+= $(extra-subdir-n)

obj-y		+= $(extra-obj-y)
obj-m		+= $(extra-obj-m)
obj-n		+= $(extra-obj-n)

#
#
#

both-m          := $(filter $(mod-subdirs), $(subdir-y))
SUB_DIRS	:= $(subdir-y)
MOD_SUB_DIRS	:= $(sort $(subdir-m) $(both-m))
ALL_SUB_DIRS	:= $(sort $(subdir-y) $(subdir-m) $(subdir-n) $(subdir-))


#
# Common rules
#

%.s: %.c
	$(CC) -D__KERNEL__ $(CFLAGS) $(EXTRA_CFLAGS) $(CFLAGS_$@) -S $< -o $@

%.i: %.c
	$(CPP) -D__KERNEL__ $(CFLAGS) $(EXTRA_CFLAGS) $(CFLAGS_$@) $< > $@

%.o: %.c
	$(CC) -D__KERNEL__ $(CFLAGS) $(EXTRA_CFLAGS) $(CFLAGS_$@) -c -o $@ $<

%.o: %.s
	$(AS) -D__KERNEL__ $(AFLAGS) $(EXTRA_CFLAGS) -o $@ $<

# Old makefiles define their own rules for compiling .S files,
# but these standard rules are available for any Makefile that
# wants to use them.  Our plan is to incrementally convert all
# the Makefiles to these standard rules.  -- rmk, mec
ifdef USE_STANDARD_AS_RULE

%.s: %.S
	$(CPP) -D__KERNEL__ $(AFLAGS) $(EXTRA_AFLAGS) $(AFLAGS_$@) $< > $@

%.o: %.S
	$(CC) -D__KERNEL__ $(AFLAGS) $(EXTRA_AFLAGS) $(AFLAGS_$@) -c -o $@ $<

endif

#
#
#
all_targets: $(O_TARGET) $(L_TARGET)

#
# Rule to compile a set of .o files into one .o file
#
ifdef O_TARGET
$(O_TARGET): $(obj-y)
	touch $@
endif # O_TARGET

#
# Rule to compile a set of .o files into one .a file
#
ifdef L_TARGET
$(L_TARGET): $(obj-y)
	touch $@
endif


#
# This make dependencies quickly
#
fastdep: $(patsubst %,_sfdep_%,$(ALL_SUB_DIRS)) update-sndversions
	$(CPP) -D__KERNEL__ $(CFLAGS) $(EXTRA_CFLAGS) $(wildcard *.[chS]) > .depend


ifneq "$(strip $(ALL_SUB_DIRS))" ""
$(patsubst %,_sfdep_%,$(ALL_SUB_DIRS)):
	$(MAKE) -C $(patsubst _sfdep_%,%,$@) fastdep
endif

#
# A rule to make subdirectories
#
subdir-list = $(sort $(patsubst %,_subdir_%,$(SUB_DIRS)))
sub_dirs: dummy $(subdir-list)

ifdef SUB_DIRS
$(subdir-list) : dummy
	$(MAKE) -C $(patsubst _subdir_%,%,$@)
endif

#
# A rule to make modules
#
ALL_MOBJS = $(filter-out $(obj-y), $(obj-m))

MOD_DIRS := $(MOD_SUB_DIRS) $(MOD_IN_SUB_DIRS)
ifneq "$(strip $(MOD_DIRS))" ""
.PHONY: $(patsubst %,_modsubdir_%,$(MOD_DIRS))
$(patsubst %,_modsubdir_%,$(MOD_DIRS)) : dummy
	$(MAKE) -C $(patsubst _modsubdir_%,%,$@) modules

.PHONY: $(patsubst %,_modinst_%,$(MOD_DIRS))
$(patsubst %,_modinst_%,$(MOD_DIRS)) : dummy
	$(MAKE) -C $(patsubst _modinst_%,%,$@) modules_install
endif

.PHONY: modules
modules: $(ALL_MOBJS) dummy \
	 $(patsubst %,_modsubdir_%,$(MOD_DIRS))

.PHONY: _modinst__
_modinst__: dummy
ifneq "$(strip $(ALL_MOBJS))" ""
	mkdir -p $(DESTDIR)$(moddir)
	cp $(ALL_MOBJS) $(DESTDIR)$(moddir)
endif

.PHONY: modules_install
modules_install: _modinst__ \
	 $(patsubst %,_modinst_%,$(MOD_DIRS))

#
# A rule to do nothing
#
dummy:

#
# This is useful for testing
#
script:
	$(SCRIPT)

#
# This sets version suffixes on exported symbols
# Separate the object into "normal" objects and "exporting" objects
# Exporting objects are: all objects that define symbol tables
#
ifdef CONFIG_MODULES

multi-used	:= $(filter $(list-multi), $(obj-y) $(obj-m))
multi-objs	:= $(foreach m, $(multi-used), $($(basename $(m))-objs))
active-objs	:= $(sort $(multi-objs) $(obj-y) $(obj-m))

MODCURDIR = $(subst $(TOPDIR)/,,$(shell /bin/pwd))

ifeq (y,$(CONFIG_SND_MVERSION))
ifneq "$(strip $(export-objs))" ""

MODINCL = $(TOPDIR)/include/modules
MODPREFIX = $(subst /,-,$(MODCURDIR))__

# The -w option (enable warnings) for genksyms will return here in 2.1
# So where has it gone?
#
# Added the SMP separator to stop module accidents between uniprocessor
# and SMP Intel boxes - AC - from bits by Michael Chastain
#

ifdef $(msmp)
	genksyms_smp_prefix := -p smp_
else
	genksyms_smp_prefix := 
endif

$(MODINCL)/$(MODPREFIX)%.ver: %.c
	@if [ ! -r $(MODINCL)/$(MODPREFIX)$*.stamp -o $(MODINCL)/$(MODPREFIX)$*.stamp -ot $< ]; then \
		echo '$(CC) -D__KERNEL__ $(CFLAGS) $(EXTRA_CFLAGS) -E -D__GENKSYMS__ $<'; \
		echo '| $(GENKSYMS) $(genksyms_smp_prefix) > $@.tmp'; \
		$(CC) -D__KERNEL__ $(CFLAGS) $(EXTRA_CFLAGS) -E -D__GENKSYMS__ $< \
		| $(GENKSYMS) $(genksyms_smp_prefix) > $@.tmp; \
		if [ -r $@ ] && cmp -s $@ $@.tmp; then echo $@ is unchanged; rm -f $@.tmp; \
		else echo mv $@.tmp $@; mv -f $@.tmp $@; fi; \
	fi; touch $(MODINCL)/$(MODPREFIX)$*.stamp

$(addprefix $(MODINCL)/$(MODPREFIX),$(export-objs:.o=.ver)): $(TOPDIR)/include/config.h $(TOPDIR)/include/config1.h

# updates .ver files but not modversions.h
fastdep: $(addprefix $(MODINCL)/$(MODPREFIX),$(export-objs:.o=.ver))

endif # export-objs 

define update-sndvers
	@(echo "#ifndef _LINUX_SNDVERSIONS_H"; \
	  echo "#define _LINUX_SNDVERSIONS_H"; \
	  echo "#include <linux/modsetver.h>"; \
	  cd $(TOPDIR)/include/modules; \
	  for f in *.ver; do \
	    if [ -f $$f ]; then echo "#include \"modules/$${f}\""; fi; \
	  done; \
	  echo "#endif"; \
	) > $(SNDVERSIONS).tmp
	@if [ -r $(SNDVERSIONS) ] && cmp -s $(SNDVERSIONS) $(SNDVERSIONS).tmp; then \
		echo $(SNDVERSIONS) was not updated; \
		rm -f $(SNDVERSIONS).tmp; \
	else \
		echo $(SNDVERSIONS) was updated; \
		mv -f $(SNDVERSIONS).tmp $(SNDVERSIONS); \
	fi
endef

$(SNDVERSIONS):
	$(update-sndvers)

$(active-objs): $(SNDVERSIONS)

$(multi-used): $(addprefix $(TOPDIR)/modules/,$(multi-used))

$(TOPDIR)/modules/%.o: dummy
	@if ! test -L $@; then \
	    echo "ln -sf ../$(MODCURDIR)/$(notdir $@) $(TOPDIR)/modules/$(notdir $@)" ; \
	    ln -sf ../$(MODCURDIR)/$(notdir $@) $(TOPDIR)/modules/$(notdir $@) ; \
	fi

else # !CONFIG_SND_MVERSION

define update-sndvers
	@echo "" > $(SNDVERSIONS)
endef

$(SNDVERSIONS):
	$(update-sndvers)

endif # CONFIG_SND_MVERSION

.PHONY: update-sndversions
update-sndversions: dummy
	$(update-sndvers)

ifneq "$(strip $(export-objs))" ""
ifeq (y,$(CONFIG_SND_MVERSION))
$(export-objs): $(addprefix $(MODINCL)/$(MODPREFIX),$(export-objs:.o=.ver)) $(export-objs:.o=.c)
else
$(export-objs): $(export-objs:.o=.c)
endif
	$(CC) -D__KERNEL__ $(CFLAGS) $(EXTRA_CFLAGS) $(CFLAGS_$@) -DEXPORT_SYMTAB -c $(@:.o=.c)
endif

endif # CONFIG_MODULES

.PHONY: clean
clean: $(patsubst %,_sfclean_%,$(ALL_SUB_DIRS))
	rm -f *.o *~

ifneq "$(strip $(ALL_SUB_DIRS))" ""
$(patsubst %,_sfclean_%,$(ALL_SUB_DIRS)):
	$(MAKE) -C $(patsubst _sfclean_%,%,$@) clean
endif

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif

ifneq ($(wildcard $(TOPDIR)/.hdepend),)
include $(TOPDIR)/.hdepend
endif
