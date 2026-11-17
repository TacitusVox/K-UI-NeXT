// SPDX-License-Identifier: GPL-3.0-only
#define _DEFAULT_SOURCE 1
#include "kui/media_tls.h"
#include <bearssl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "media_tls_roots.h"

struct kui_media_tls {
    br_ssl_client_context client;
    br_x509_minimal_context verifier;
    unsigned char record[BR_SSL_BUFSIZE_MONO];
    const char *failure;
};

/* Use the library's constant-time primitives. TLS 1.2 with ephemeral keys
 * and AEAD suits these public HTTP/1.1 origins and omits legacy algorithms. */
static void configure(struct kui_media_tls *tls) {
    static const uint16_t suites[] = {
        BR_TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
        BR_TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
        BR_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
        BR_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
        BR_TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
        BR_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384
    };
    static const br_hash_class *const hashes[] = {
        &br_sha256_vtable, &br_sha384_vtable, &br_sha512_vtable
    };
    br_ssl_engine_context *engine = &tls->client.eng;
    br_ssl_client_zero(&tls->client);
    br_ssl_engine_set_versions(engine, BR_TLS12, BR_TLS12);
    br_ssl_engine_set_suites(engine, suites, sizeof(suites) / sizeof(suites[0]));
    br_ssl_engine_set_default_rsavrfy(engine);
    br_ssl_engine_set_default_ecdsa(engine);
    br_x509_minimal_init(&tls->verifier, &br_sha256_vtable, TAs, TAs_NUM);
    br_x509_minimal_set_minrsa(&tls->verifier, 256); /* 2048 bits */
    br_x509_minimal_set_rsa(&tls->verifier, br_ssl_engine_get_rsavrfy(engine));
    br_x509_minimal_set_ecdsa(&tls->verifier, br_ssl_engine_get_ec(engine),
                              br_ssl_engine_get_ecdsa(engine));
    for(size_t i = 0; i < sizeof(hashes) / sizeof(hashes[0]); ++i) {
        int id = (int)(i + br_sha256_ID);
        br_ssl_engine_set_hash(engine, id, hashes[i]);
        br_x509_minimal_set_hash(&tls->verifier, id, hashes[i]);
    }
    br_ssl_engine_set_x509(engine, &tls->verifier.vtable);
    br_ssl_engine_set_prf_sha256(engine, &br_tls12_sha256_prf);
    br_ssl_engine_set_prf_sha384(engine, &br_tls12_sha384_prf);
    br_ssl_engine_set_default_aes_gcm(engine);
    br_ssl_engine_set_default_chapol(engine);
    br_ssl_engine_set_buffer(engine, tls->record, sizeof(tls->record), 0);
}

kui_media_tls_t *kui_media_tls_open(const char *hostname) {
    if(!hostname || !*hostname || strlen(hostname) > 253)
        return NULL;
    struct kui_media_tls *tls = calloc(1, sizeof(*tls));
    if(!tls) return NULL;
    time_t now = time(NULL);
    if(now <= 0) {
        tls->failure = "Set the Dreamcast clock for HTTPS";
        return tls;
    }
    configure(tls);
    /* BearSSL uses days since 0000-01-01; time() is Unix epoch seconds. */
    uint64_t epoch = (uint64_t)now;
    br_x509_minimal_set_time(&tls->verifier, (uint32_t)(epoch / 86400) + 719528u,
                             (uint32_t)(epoch % 86400));
    unsigned char seed[32];
    arc4random_buf(seed, sizeof(seed));
    br_ssl_engine_inject_entropy(&tls->client.eng, seed, sizeof(seed));
    explicit_bzero(seed, sizeof(seed));
    if(!br_ssl_client_reset(&tls->client, hostname, 0))
        tls->failure = "Could not start HTTPS";
    return tls;
}

void kui_media_tls_free(kui_media_tls_t *tls) {
    if(tls) explicit_bzero(tls, sizeof(*tls));
    free(tls);
}

const char *kui_media_tls_error(const kui_media_tls_t *tls) {
    if(!tls) return "Could not allocate HTTPS client";
    if(tls->failure) return tls->failure;
    int err = br_ssl_engine_last_error(&tls->client.eng);
    switch(err) {
    case BR_ERR_X509_EXPIRED:
    case BR_ERR_X509_TIME_UNKNOWN:
        return "Check Dreamcast clock / certificate date";
    case BR_ERR_X509_BAD_SERVER_NAME:
        return "HTTPS certificate hostname mismatch";
    case BR_ERR_X509_NOT_TRUSTED:
        return "HTTPS certificate authority untrusted";
    default:
        return err >= BR_ERR_X509_INVALID_VALUE && err <= BR_ERR_X509_NOT_TRUSTED
            ? "HTTPS certificate validation failed" : "HTTPS connection failed";
    }
}

int kui_media_tls_pump(kui_media_tls_t *tls, void *ctx,
                       int (*recv_wire)(void *, void *, size_t),
                       int (*send_wire)(void *, const void *, size_t)) {
    if(!tls || tls->failure || !recv_wire || !send_wire) return -1;
    br_ssl_engine_context *engine = &tls->client.eng;
    unsigned state = br_ssl_engine_current_state(engine);
    if(state & BR_SSL_CLOSED)
        return br_ssl_engine_last_error(engine) == BR_ERR_OK ? -2 : -1;
    size_t available;
    unsigned char *buffer;
    int n;
    if(state & BR_SSL_SENDREC) {
        buffer = br_ssl_engine_sendrec_buf(engine, &available);
        n = send_wire(ctx, buffer, available);
        if(n > 0 && (size_t)n <= available) {
            br_ssl_engine_sendrec_ack(engine, (size_t)n);
            return 1;
        }
    } else if(state & BR_SSL_RECVREC) {
        buffer = br_ssl_engine_recvrec_buf(engine, &available);
        n = recv_wire(ctx, buffer, available);
        if(n > 0 && (size_t)n <= available) {
            br_ssl_engine_recvrec_ack(engine, (size_t)n);
            return 1;
        }
    } else {
        return 0;
    }
    if(n == 0) return 0;
    tls->failure = n == -2 ? "HTTPS connection ended early" : "HTTPS transport error";
    return -1;
}

int kui_media_tls_read(kui_media_tls_t *tls, void *dest, size_t size) {
    if(!tls || tls->failure || !dest || size == 0) return 0;
    size_t available;
    const unsigned char *buf = br_ssl_engine_recvapp_buf(&tls->client.eng, &available);
    if(!buf) return 0;
    if(size > available) size = available;
    memcpy(dest, buf, size);
    br_ssl_engine_recvapp_ack(&tls->client.eng, size);
    return (int)size;
}

int kui_media_tls_write(kui_media_tls_t *tls, const void *source, size_t size) {
    if(!tls || tls->failure || !source || size == 0) return 0;
    size_t available;
    unsigned char *buf = br_ssl_engine_sendapp_buf(&tls->client.eng, &available);
    if(!buf) return 0;
    if(size > available) size = available;
    memcpy(buf, source, size);
    br_ssl_engine_sendapp_ack(&tls->client.eng, size);
    br_ssl_engine_flush(&tls->client.eng, 0);
    return (int)size;
}
