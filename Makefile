VERSION := 1.14

ifdef CROSS_COMPILE
	CC := $(CROSS_COMPILE)-gcc
	OBJCOPY := $(CROSS_COMPILE)-objcopy
endif

ifndef CC
	CC := cc
endif
ifndef OBJCOPY
	OBJCOPY := objcopy
endif

CFLAGS := -O2 -Wall
EXT_LIBS := -l uci -l ubox

#получим нашу корневую директорию
ROOT_DIR := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))

EXTRA_CFLAGS += -DVERSION='"$(VERSION)"'

mtpoe_ctrl-h := mtpoe_ctrl.h mk_com.h params.h signals.h
mtpoe_ctrl-objs-c := mk_com.c
mtpoe_ctrl-objs := $(mtpoe_ctrl-objs-c:%.c=objs/%.o)
mtpoe_ctrl-bins-c := mtpoe_ctrl.c
mtpoe_ctrl-bins := $(mtpoe_ctrl-bins-c:%.c=bins/%)

#all идет первой и привязана к бинарникам bins/*
all: $(mtpoe_ctrl-bins)

#зависимости
$(mtpoe_ctrl-h):
$(mtpoe_ctrl-objs): $(mtpoe_ctrl-h)
$(mtpoe_ctrl-bins): $(mtpoe_ctrl-objs)

$(mtpoe_ctrl-bins): bins/%: %.c
	@mkdir -p bins
	$(CC) $(CFLAGS) $(LDFLAGS) $(EXTRA_CFLAGS) $< -o $@ $(EXT_LIBS) $(mtpoe_ctrl-objs)
	$(OBJCOPY) --strip-all $@ $@

$(mtpoe_ctrl-objs): objs/%.o: %.c
	@mkdir -p objs
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $< -c -o $@

clean:
	rm -f bins/* objs/*

#для отладки
mips:
	$(eval OPENWRT_SOURCE := /home/prog/openwrt/2020-openwrt/openwrt-18.06.8)
	$(eval TARGET_DIR := $(OPENWRT_SOURCE)/staging_dir/target-mips_24kc_musl)
	@STAGING_DIR=$(OPENWRT_SOURCE)/staging_dir/toolchain-mips_24kc_gcc-7.3.0_musl \
	CROSS_COMPILE=$$STAGING_DIR/bin/mips-openwrt-linux \
	EXTRA_CFLAGS="-I${TARGET_DIR}/usr/include -L${TARGET_DIR}/usr/lib" \
	make all

#для отладки
mips_nc:
	make mips
	@echo ""
	@echo "Do NetCat port port 1111"
	@cat ./bins/mtpoe_ctrl | nc -l -p 1111 -q 1
