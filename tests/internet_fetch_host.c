/* SPDX-License-Identifier: GPL-3.0-only */
/* Native HTTPS client + production BBA TCP sockets. The optional fixture
 * port routes a test DNS name to an ephemeral loopback TLS listener. */
#define _POSIX_C_SOURCE 200809L
#include "../src/core/internet_internal.h"
#include "kui/bba_sockets.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static uint16_t fixture_port;
static bool pending_init;
static unsigned connects;
static bool (*original_state)(void *,unsigned,uint8_t *);
static bool fixture_state(void *ctx,unsigned slot,uint8_t *state) {
    if(pending_init) {pending_init=false;*state=KUI_NET_INIT;return true;}
    return original_state(ctx,slot,state);
}
static bool (*original_connect)(void *,unsigned,uint16_t,const uint8_t [4],uint16_t);
static bool connect_fixture(void *ctx,unsigned slot,uint16_t local,const uint8_t ip[4],uint16_t port) {
    ++connects;
    pending_init=fixture_port!=0; /* W5500 may briefly report INIT during ARP. */
    return original_connect(ctx,slot,local,ip,fixture_port ? fixture_port : port);
}
static uint64_t millis(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC,&t);
    return (uint64_t)t.tv_sec*1000u+(unsigned long)t.tv_nsec/1000000u;
}
static bool cancelled,link_down;
static void nap(unsigned ms) {
    if(ms==25 && getenv("KUI_TEST_CANCEL_BACKOFF")) cancelled=true;
    if(ms==25 && getenv("KUI_TEST_DROP_LINK")) link_down=true;
    struct timespec t={ms/1000u,(long)(ms%1000u)*1000000L};
    nanosleep(&t,NULL);
}
static bool dns(struct kui_internet *s,const char *host,uint8_t ip[4],const uint8_t avoid[4]) {
    (void)s;
    if(fixture_port) {
        if(strcmp(host,"media.example.invalid") && strcmp(host,"other.example.invalid")) return false;
        memcpy(ip,"\x7f\0\0\1",4);
        const char *failure=getenv("KUI_TEST_CONNECT");
        if(failure) {
            if(avoid && avoid[3]!=2) return false;
            if(!avoid || !strcmp(failure,"persistent")) ip[3]=2;
        }
        return true;
    }
    struct addrinfo hints={.ai_family=AF_INET,.ai_socktype=SOCK_STREAM},*result=NULL;
    if(getaddrinfo(host,NULL,&hints,&result)) return false;
    memcpy(ip,&((struct sockaddr_in *)result->ai_addr)->sin_addr,4);
    freeaddrinfo(result);
    return true;
}
static bool online(struct kui_internet *s) {(void)s; return !link_down;}
static bool stopped(void) {return cancelled;}
static unsigned after,done,prefix;
static bool complete(void *ctx) {(void)ctx;return done>=prefix;}
static bool save(void *f,const void *data,size_t size) {
    done+=(unsigned)size;
    if(after && done>=after) cancelled=true;
    return fwrite(data,1,size,f)==size;
}
int main(int argc,char **argv) {
    if(argc<3 || argc>7) {
        fprintf(stderr,"usage: %s URL OUTPUT [limit [loopback-port [cancel-after-bytes [prefix-bytes]]]]\n",argv[0]); return 2;
    }
    struct kui_internet s={.poll=online,.resolve=dns,.now=millis,.pause=nap,.cancel=stopped,.adapter="host TCP"};
    if(!kui_bba_sockets_init(&s.net)) return 2;
    if(argc>4) fixture_port=(uint16_t)strtoul(argv[4],NULL,10);
    if(argc>5) after=(unsigned)strtoul(argv[5],NULL,10);
    if(argc>6) prefix=(unsigned)strtoul(argv[6],NULL,10);
    original_connect=s.net.connect;s.net.connect=connect_fixture;
    original_state=s.net.state;s.net.state=fixture_state;
    FILE *f=fopen(argv[2],"wb");if(!f) return 2;
    uint32_t limit=argc>3 ? (uint32_t)strtoul(argv[3],NULL,10) : UINT32_C(33554432);
    const char *range=getenv("KUI_TEST_RANGE");
    const char *metadata=getenv("KUI_TEST_VALIDATORS");
    enum kui_http_result r;
    if(metadata) {
        struct kui_http_validators validators={0};
        FILE *cache=fopen(metadata,"rb");
        if(cache) {if(fread(&validators,1,sizeof(validators),cache)!=sizeof(validators) || fclose(cache)) return 2;}
        r=kui_internet_get_conditional(&s,argv[1],limit,save,f,NULL,NULL,&validators,&validators);
        if(r==KUI_HTTP_OK || r==KUI_HTTP_NOT_MODIFIED) {
            cache=fopen(metadata,"wb");
            if(!cache || fwrite(&validators,1,sizeof(validators),cache)!=sizeof(validators) || fclose(cache)) return 2;
        }
    } else r=range ? kui_internet_get_remainder(&s,argv[1],(uint32_t)strtoul(range,NULL,10),limit,save,f,NULL,NULL) :
        prefix ? kui_internet_get_prefix(&s,argv[1],limit,save,f,NULL,NULL,complete) :
        kui_internet_get(&s,argv[1],limit,save,f,NULL,NULL);
    if(fclose(f)) return 2;
    kui_bba_sockets_end(&s.net);
    const char *expected=getenv("KUI_TEST_ATTEMPTS");
    if(expected && connects!=strtoul(expected,NULL,10)) {
        fprintf(stderr,"Expected %s TCP attempts, saw %u\n",expected,connects);
        return 2;
    }
    printf("%u %u %s\n",r,done,s.problem);
    return r==KUI_HTTP_OK ? 0 : (int)r+10;
}
