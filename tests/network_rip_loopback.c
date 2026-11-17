/* SPDX-License-Identifier: GPL-3.0-only */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include "network_rip_posix.h"
#include "kui/bba_sockets.h"
/* Keep the production capture requirement for a LAN receiver address.
 * Redirect that one test address into the private loopback receiver. */
static bool (*bba_connect)(void *,unsigned,uint16_t,const uint8_t[4],uint16_t);
static bool connect_receiver(void *ctx,unsigned slot,uint16_t local,const uint8_t ip[4],uint16_t port){
    assert(!memcmp(ip,(uint8_t[]){10,0,0,2},4));
    return bba_connect(ctx,slot,local,(uint8_t[]){127,0,0,1},port);
}
int main(int argc,char **argv){
    if(argc!=5)return 2;
    struct kui_network_rip_config config;nr_config(&config,argv[1],argv[2],argv[3]);
    struct nr_posix posix={.fd=-1,.window=7};struct kui_net_sockets net=nr_sockets(&posix);
    bool bba=!strcmp(argv[4],"bba");
    if(bba){
        assert(kui_bba_sockets_init(&net));
        bba_connect=net.connect;net.connect=connect_receiver;
    }
    struct kui_network_rip_hooks hooks={.now_ms=nr_now,.pause=nr_pause};
    struct kui_network_rip *client=calloc(1,sizeof(*client));assert(client);
    bool open=kui_network_rip_open(client,&net,0,&config,&hooks);
    if(!bba&&strcmp(argv[4],"io")){
        assert(!open&&!client->live&&!client->connected&&client->problem[0]);
        struct kui_media_ops media;kui_network_rip_media(client,&media);assert(!media.read&&!media.write&&!media.blocks);
        kui_network_rip_close(client);free(client);puts("PASS refused remote destination");return 0;
    }
    if(!open){fprintf(stderr,"%s\n",client->problem);abort();}
    struct kui_media_ops media;kui_network_rip_media(client,&media);assert(media.blocks(media.ctx)>100);
    uint8_t write[40*512],read[40*512];
    for(size_t i=0;i<sizeof(write);i++)write[i]=(uint8_t)(i*37u+(i/512u));
    assert(!media.write(media.ctx,10,40,write));assert(!media.sync(media.ctx));
    memset(read,0,sizeof(read));assert(!media.read(media.ctx,10,40,read)&&!memcmp(read,write,sizeof(read)));
    assert(media.read(media.ctx,0,0,read));assert(media.write(media.ctx,UINT32_MAX,2,write));assert(client->live);
    kui_network_rip_close(client);assert(!client->live&&!client->connected);
    for(size_t i=0;i<sizeof(client->config.key);i++)assert(!client->config.key[i]);
    assert(kui_network_rip_open(client,&net,0,&config,&hooks));kui_network_rip_media(client,&media);
    assert(!media.read(media.ctx,10,40,read)&&!memcmp(read,write,sizeof(read)));
    kui_network_rip_close(client);free(client);
    if(bba){kui_bba_sockets_end(&net);puts("PASS BBA authenticated remote I/O, sync and reconnect");return 0;}
    puts("PASS fragmented authenticated remote random I/O, sync and reconnect");return 0;
}
