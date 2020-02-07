proj := qrtr
proj-major := 1
proj-minor := 0
proj-version := $(proj-major).$(proj-minor)

CFLAGS := -Wall -g
LDFLAGS :=

prefix := /usr/local
bindir := $(prefix)/bin
libdir := $(prefix)/lib
includedir := $(prefix)/include
servicedir := $(prefix)/lib/systemd/system

ifneq ($(CROSS_COMPILE),)
CC := $(CROSS_COMPILE)gcc
endif
SFLAGS := -I$(shell $(CC) -print-file-name=include) -Wno-non-pointer-null

$(proj)-cfg-srcs := \
	lib/logging.c \
	src/addr.c \
	src/cfg.c \

$(proj)-cfg-cflags := -Ilib

$(proj)-ns-srcs := \
	lib/logging.c \
	src/addr.c \
	src/ns.c \
	src/map.c \
	src/hash.c \
	src/waiter.c \
	src/util.c \

$(proj)-ns-cflags := -Ilib

$(proj)-lookup-srcs := \
	lib/logging.c \
	src/lookup.c \
	src/util.c \

$(proj)-lookup-cflags := -Ilib

lib$(proj).so-srcs := \
	lib/logging.c \
	lib/qrtr.c \
	lib/qmi.c

lib$(proj).so-cflags := -fPIC -Isrc

targets := $(proj)-ns $(proj)-cfg $(proj)-lookup lib$(proj).so

out := out
src_to_obj = $(patsubst %.c,$(out)/obj/%.o,$(1))
src_to_dep = $(patsubst %.c,$(out)/dep/%.d,$(1))

all-srcs :=
all-objs :=
all-deps :=
all-clean := $(out)
all-install :=

all: $(targets)

$(out)/obj/%.o: %.c
ifneq ($C,)
	@echo "CHECK	$<"
	@sparse $< $(patsubst -iquote=%,-I%,$(CFLAGS)) $(SFLAGS)
endif
	@echo "CC	$<"
	@$(CC) -MM -MF $(call src_to_dep,$<) -MP -MT "$@ $(call src_to_dep,$<)" $(CFLAGS) $(_CFLAGS) $<
	@$(CC) -o $@ -c $< $(CFLAGS) $(_CFLAGS)

define add-inc-target
$(DESTDIR)$(includedir)/$2: $1/$2
	@echo "INSTALL	$$<"
	@install -D -m 755 $$< $$@

all-install += $(DESTDIR)$(includedir)/$2
endef

define add-target-deps
all-srcs += $($1-srcs)
all-objs += $(call src_to_obj,$($1-srcs))
all-deps += $(call src_to_dep,$($1-srcs))
all-clean += $1
$(call src_to_obj,$($1-srcs)): _CFLAGS := $($1-cflags)
endef

define add-bin-target

$(call add-target-deps,$1)

$1: $(call src_to_obj,$($1-srcs))
	@echo "LD	$$@"
	$$(CC) -o $$@ $$(filter %.o,$$^) $(LDFLAGS)

$(DESTDIR)$(bindir)/$1: $1
	@echo "INSTALL	$$<"
	@install -D -m 755 $$< $$@

all-install += $(DESTDIR)$(bindir)/$1
endef

define add-lib-target

$(call add-target-deps,$1)

$1: $(call src_to_obj,$($1-srcs))
	@echo "LD	$$@"
	$$(CC) -o $$@ $$(filter %.o,$$^) $(LDFLAGS) -shared -Wl,-soname,$1.$(proj-major)

$(DESTDIR)$(libdir)/$1.$(proj-version): $1
	@echo "INSTALL	$$<"
	@install -D -m 755 $$< $$@
	@ln -sf $1.$(proj-version) $(DESTDIR)$(libdir)/$1.$(proj-major)
	@ln -sf $1.$(proj-major) $(DESTDIR)$(libdir)/$1

all-install += $(DESTDIR)$(libdir)/$1.$(proj-version)
endef

define add-systemd-service-target
$1: $1.in
	sed 's+QRTR_NS_PATH+$(bindir)+g' $$< > $$@

$(DESTDIR)$(servicedir)/$1: $1
	@echo "INSTALL	$$<"
	@install -D -m 755 $$< $$@

all-install += $(DESTDIR)$(servicedir)/$1
endef

$(foreach v,$(filter-out %.so,$(targets)),$(eval $(call add-bin-target,$v)))
$(foreach v,$(filter %.so,$(targets)),$(eval $(call add-lib-target,$v)))
$(eval $(call add-inc-target,lib,libqrtr.h))
$(eval $(call add-systemd-service-target,qrtr-ns.service))

install: $(all-install)

clean:
	@echo CLEAN
	@$(RM) -r $(all-clean)

$(call src_to_obj,$(all-srcs)): Makefile

ifneq ("$(MAKECMDGOALS)","clean")
cmd-goal-1 := $(shell mkdir -p $(sort $(dir $(all-objs) $(all-deps))))
-include $(all-deps)
endif
