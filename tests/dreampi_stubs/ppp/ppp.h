/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
#include <sys/types.h>
typedef struct ppp_device {
    const char *name, *descr;
    int (*detect)(struct ppp_device *), (*init)(struct ppp_device *), (*shutdown)(struct ppp_device *);
    int (*tx)(struct ppp_device *, const uint8_t *, size_t, uint32_t);
    const uint8_t *(*rx)(struct ppp_device *, ssize_t *);
} ppp_device_t;
int ppp_init(void);
int ppp_shutdown(void);
int ppp_set_device(ppp_device_t *device);
int ppp_set_login(const char *user, const char *password);
