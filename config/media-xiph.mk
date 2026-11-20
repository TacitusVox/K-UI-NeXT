# SPDX-License-Identifier: GPL-3.0-only
XIPH_DEP := build/media-xiph/ogg-1.3.6-theora-1.2.0.ready
XIPH_OGG := .deps/libogg-1.3.6
XIPH_THEORA := .deps/libtheora-1.2.0
XIPH_FLAGS := -I$(XIPH_OGG)/include -I$(XIPH_THEORA)/include -Iconfig/media-xiph
XIPH_LIBFLAGS := $(XIPH_FLAGS) -O2 -include config/media-xiph/alloc.h
XIPH_OGG_UNITS := bitwise framing
XIPH_THEORA_UNITS := bitpack decinfo decode dequant fragment huffdec idct info internal quant state
XIPH_UNITS := $(addprefix ogg/,$(XIPH_OGG_UNITS)) $(addprefix theora/,$(XIPH_THEORA_UNITS))
XIPH_DC_OBJS := $(addprefix build/media-xiph/dc/,$(addsuffix .o,$(XIPH_UNITS)))
XIPH_HOST_OBJS := $(addprefix build/media-xiph/host/,$(addsuffix .o,$(XIPH_UNITS)))

$(XIPH_DEP): tools/fetch_media_xiph.py
	python3 tools/fetch_media_xiph.py
	@mkdir -p $(@D)
	@touch $@

# The release downloads need not already exist when Make reads these patterns.
define xiph-rules
build/media-xiph/$(1)/ogg/%.o: $(XIPH_DEP) config/media-xiph/alloc.h config/media-xiph/ogg/config_types.h
	@mkdir -p $$(@D)
	$(2) $(XIPH_LIBFLAGS) -c $(XIPH_OGG)/src/$$*.c -o $$@
build/media-xiph/$(1)/theora/%.o: $(XIPH_DEP) config/media-xiph/alloc.h config/media-xiph/ogg/config_types.h
	@mkdir -p $$(@D)
	$(2) $(XIPH_LIBFLAGS) -c $(XIPH_THEORA)/lib/$$*.c -o $$@
endef
$(eval $(call xiph-rules,dc,kos-cc))
$(eval $(call xiph-rules,host,$(CC)))

build/dc/src/apps/internet_preview.o: CPPFLAGS += $(XIPH_FLAGS)
build/dc/src/apps/internet_preview.o: $(XIPH_DEP)

build/test-internet-preview: tests/internet_preview_host.c src/apps/internet_preview.c src/apps/music_ogg.c src/core/hash.c src/core/data.c include/kui/internet_preview.h include/kui/preview_prefix.h include/kui/music_ogg.h include/kui/media_container.h $(XIPH_HOST_OBJS) $(XIPH_DEP)
	$(CC) -std=c11 -Wall -Wextra -Werror -O2 -Iinclude -I.deps/fatfs/source $(XIPH_FLAGS) \
	    -Isrc/dreamcast -Itests/stubs tests/internet_preview_host.c src/apps/internet_preview.c src/apps/music_ogg.c src/core/hash.c src/core/data.c $(XIPH_HOST_OBJS) -lm -o $@

.PHONY: test-internet-preview
test-internet-preview: build/test-internet-preview build/test-preview-prefix
	python3 tests/test_internet_preview.py

build/test-internet-extra: tests/test_internet_extra.c src/apps/internet_extra.c src/apps/internet_cover.c \
        src/apps/internet_extra_data.inc src/apps/internet_preview_regions.inc src/apps/internet_cover_catalog.inc src/apps/internet_cover_small.inc
	@mkdir -p $(@D)
	$(CC) -std=c11 -Wall -Wextra -Werror -O2 -Iinclude -I.deps/fatfs/source \
	    tests/test_internet_extra.c src/apps/internet_extra.c src/apps/internet_cover.c -o $@

.PHONY: test-internet-extra
test-internet-extra: build/test-internet-extra
	./build/test-internet-extra
	python3 tests/test_preview_regions.py

# Makefile.dc includes dependency files for all GUI objects. Ogg/Theora
# sources are immutable at this pin; do not look for derived *.d.c inputs.
$(XIPH_DC_OBJS:.o=.d): ;
