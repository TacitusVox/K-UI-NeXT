# SPDX-License-Identifier: GPL-3.0-only
# Only the GUI needs this client. The bootloader and game payloads keep their
# existing memory maps. Host and SH-4 objects always use separate directories.
MEDIA_TLS_PIN := $(shell python3 -c 'import json; print(json.load(open("dependencies.json"))["bearssl"]["commit"])')
MEDIA_TLS_DEP := build/bearssl/$(MEDIA_TLS_PIN).ready
MEDIA_TLS_HOST := $(abspath build/bearssl/host-$(MEDIA_TLS_PIN))
MEDIA_TLS_DC := $(abspath build/bearssl/dc-$(MEDIA_TLS_PIN))
MEDIA_TLS_DC_LIB := $(MEDIA_TLS_DC)/libbearssl.a
MEDIA_TLS_ROOTS := build/bearssl/media_tls_roots.h

$(MEDIA_TLS_DEP): dependencies.json tools/fetch_deps.py
	python3 tools/fetch_deps.py --bearssl-only
	@mkdir -p $(@D)
	@touch $@

$(MEDIA_TLS_HOST)/brssl: $(MEDIA_TLS_DEP)
	+$(MAKE) -C .deps/bearssl BUILD=$(MEDIA_TLS_HOST) tools

$(MEDIA_TLS_ROOTS): $(MEDIA_TLS_HOST)/brssl config/media-public-roots.pem
	$(MEDIA_TLS_HOST)/brssl ta -q config/media-public-roots.pem > $@.tmp
	mv $@.tmp $@

$(MEDIA_TLS_DC_LIB): $(MEDIA_TLS_DEP)
	+$(MAKE) -C .deps/bearssl BUILD=$(MEDIA_TLS_DC) CC=kos-cc AR=$(KOS_AR) \
		CFLAGS="-Os -DBR_USE_URANDOM=0 -DBR_USE_UNIX_TIME=0" lib

.PHONY: media-tls-host
media-tls-host: $(MEDIA_TLS_ROOTS)
