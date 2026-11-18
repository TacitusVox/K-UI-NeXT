# SPDX-License-Identifier: GPL-3.0-only
# The media worker starts and stops PPP within a running GUI.
KUI_PPP := $(KOS_BASE)/addons/libppp
KUI_MODEM := $(KOS_BASE)/kernel/arch/dreamcast/hardware/modem
RUNTIME_OBJECTS += build/dc/src/dreamcast/dreampi.o build/dc/kos-ppp-session.o \
    build/dc/kos-ppp-net.o build/dc/kos-modem-irq.o \
    $(addprefix build/dc/kos-ppp-,lcp.o pap.o ipcp.o)
build/dc/kos-ppp-session.c: $(KUI_PPP)/ppp.c tools/patches/kos-ppp-session.patch
	@mkdir -p $(@D)
	patch --batch --fuzz=0 -o $@.tmp $< $(word 2,$^) && mv $@.tmp $@
build/dc/kos-ppp-net.c: $(KOS_BASE)/kernel/net/net_core.c tools/patches/kos-ppp-no-dhcp.patch
	@mkdir -p $(@D)
	patch --batch --fuzz=0 -o $@.tmp $< $(word 2,$^) && mv $@.tmp $@
build/dc/kos-modem-irq.c: $(KUI_MODEM)/mintr.c tools/patches/kos-modem-session.patch
	@mkdir -p $(@D)
	patch --batch --fuzz=0 -o $@.tmp $< $(word 2,$^) && mv $@.tmp $@
build/dc/kos-ppp-session.o: build/dc/kos-ppp-session.c
	kos-cc -std=gnu11 -I$(KUI_PPP) -I$(KOS_BASE)/kernel/net -MMD -MP -c $< -o $@
build/dc/kos-ppp-net.o: build/dc/kos-ppp-net.c
	kos-cc -std=gnu11 -I$(KOS_BASE)/kernel/net -MMD -MP -c $< -o $@
build/dc/kos-modem-irq.o: build/dc/kos-modem-irq.c
	kos-cc -std=gnu11 -I$(KUI_MODEM) -MMD -MP -c $< -o $@
build/dc/kos-ppp-%.o: $(KUI_PPP)/%.c
	@mkdir -p $(@D)
	kos-cc -std=gnu11 -I$(KOS_BASE)/kernel/net -MMD -MP -c $< -o $@
