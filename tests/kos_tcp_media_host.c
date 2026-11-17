/* SPDX-License-Identifier: GPL-3.0-only */
/* KOS protocol logic runs unchanged; substitute its clock, wire, locks and job
 * scheduler. Files included below are extracted by test_kos_tcp_media.py. */
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/socket.h>

typedef int file_t, mutex_t, condvar_t, rw_semaphore_t, netif_t, workqueue_t;
typedef struct { uint64_t time_ms; } workqueue_job_t;
typedef struct { void *data; } net_socket_t;
#define RWSEM_INITIALIZER 0
#define __packed __attribute__((packed))
#define __s6_addr __in6_u
#define __s6_addr32 __u6_addr32
#define FILEHND_INVALID -1
#define FS_SOCKET_NONBLOCK 1
/* KOS poll.h aliases normal write/read to POLLOUT/POLLIN. */
#undef POLLWRNORM
#define POLLWRNORM POLLOUT
#undef POLLRDNORM
#define POLLRDNORM POLLIN
#define NET_IPV4_FRAME_HDR_SIZE 34
#define __fallthrough __attribute__((fallthrough))
#include "kos_tcp_types.h"

static uint64_t now;
static unsigned acks, fins, data_packets, last_data_size;
static uint32_t last_seq, last_ack;
static uint64_t timer_ms_gettime64(void) { return now; }
#define mutex_destroy(x) (void)(x)
#define mutex_unlock(x) (void)(x)
#define mutex_lock_irqsafe(x) ((void)(x),0)
#define mutex_lock_scoped(x) (void)(x)
#define cond_signal(x) (void)(x)
#define cond_destroy(x) (void)(x)
#define cond_wait(cv,mu) assert(0)
#define rwsem_read_lock(x) (void)(x)
#define rwsem_read_unlock(x) (void)(x)
#define rwsem_write_lock(x) (void)(x)
#define rwsem_write_unlock(x) (void)(x)
#define rwsem_read_lock_irqsafe(x) ((void)(x),0)
#define net_tcp_read_lock_and_get_sock(hnd, sem) ((struct tcp_sock *)(hnd)->data)
#define net_tcp_write_lock_and_get_sock(hnd, sem) net_tcp_read_lock_and_get_sock(hnd,sem)
#define thd_pass() assert(0)
#define irq_inside_int() 0
#define dbglog(...) (void)0
#define workqueue_enqueue(wq,job) ((void)(wq), (void)(job))
#define net_ipv6_checksum_pseudo(...) 0
#define net_ipv4_checksum(wire,sz,seed) ((void)(seed),0)
#define tcp_rst(...) (void)ls
#define tcp_bpkt_rst(net,src,dst,hdr,size) ((void)(src),(void)(dst))
#define tcp_send_syn(sock,ack) ((void)(sock),(void)(ack))
static int capture(const uint8_t *data, size_t size) {
    const tcp_hdr_t *hdr = (const tcp_hdr_t *)data;
    unsigned flags = ntohs(hdr->off_flags);
    assert(flags & TCP_FLAG_ACK);
    last_seq = ntohl(hdr->seq);
    last_ack = ntohl(hdr->ack);
    if(flags & TCP_FLAG_FIN) ++fins;
    else if(size == sizeof(*hdr)) ++acks;
    else { ++data_packets; last_data_size = size-sizeof(*hdr); }
    return 0;
}
#define net_ipv4_send_inplace(net,frame,sz,id,hops,tos,proto,src,dst) ((void)(src),(void)(dst),capture((frame)+34,sz))
#define net_ipv6_send(net,wire,sz,...) capture(wire,sz)
#include "kos_tcp_routines.h"

static struct tcp_sock *connection(int state, uint32_t next) {
    struct tcp_sock *s = calloc(1,sizeof(*s)); assert(s);
    s->state = state; s->sock = 7;
    s->data.sndbuf = malloc(TCP_DEFAULT_WINDOW);
    s->data.rcvbuf = malloc(TCP_DEFAULT_WINDOW);
    assert(s->data.sndbuf && s->data.rcvbuf);
    s->sndbuf_sz = s->rcvbuf_sz = s->data.rcv.wnd = s->data.snd.wnd = TCP_DEFAULT_WINDOW;
    s->data.snd.mss = 1460; s->data.rcv.nxt = 1000;
    s->data.snd.nxt = s->data.snd.una = next;
    s->flags = FS_SOCKET_NONBLOCK;
    LIST_INSERT_HEAD(&tcp_socks,s,sock_list);
    return s;
}
static void packet(struct tcp_sock *s,unsigned flags,uint32_t seq,uint32_t ack) {
    flags &= 0xffu;
    tcp_hdr_t hdr={.off_flags=htons(flags|TCP_OFFSET(5)),.seq=htonl(seq),
                   .ack=htonl(ack),.wnd=htons(TCP_DEFAULT_WINDOW)};
    assert(process_pkt(NULL,NULL,NULL,&hdr,s,flags|TCP_OFFSET(5),sizeof(hdr))==0);
}
static void tick(unsigned ms) {
    workqueue_job_t job;
    now+=ms;
    net_tcp_job(NULL,&job);
}
static void descriptor_close(struct tcp_sock *s) {
    net_socket_t h = {s};
    net_tcp_close(&h);
    assert(s->sock == FILEHND_INVALID);
}
static void passive(void) {
    /* Server sends its FIN first. K-UI still has an open descriptor and can
     * read data before closing, then completes LAST_ACK on the final ACK. */
    struct tcp_sock *s = connection(TCP_STATE_ESTABLISHED,42);
    packet(s,TCP_FLAG_FIN|TCP_FLAG_ACK,1000,42);
    assert(s->state==TCP_STATE_CLOSE_WAIT && last_ack==1001);
    descriptor_close(s);
    assert(s->state==TCP_STATE_LAST_ACK && last_seq==42 && s->data.snd.nxt==43);
    unsigned sent = fins; tick(TCP_DEFAULT_RTTO);
    assert(fins==sent+1 && last_seq==42 && s->data.snd.nxt==43);
    packet(s,TCP_FLAG_ACK,1001,43);
    assert(s->state==TCP_STATE_CLOSED && s->data.sndbuf_cur_sz==0);
    tick(10);
    assert(LIST_EMPTY(&tcp_socks));
}
static void active(bool simultaneous) {
    struct tcp_sock *s = connection(TCP_STATE_ESTABLISHED,UINT32_MAX);
    descriptor_close(s);
    assert(s->state==TCP_STATE_FIN_WAIT_1 && last_seq==UINT32_MAX);
    if(simultaneous) {
        packet(s,TCP_FLAG_FIN|TCP_FLAG_ACK,1000,UINT32_MAX);
        assert(s->state==TCP_STATE_CLOSING);
        tick(TCP_DEFAULT_RTTO);
        assert(last_seq==UINT32_MAX);
        packet(s,TCP_FLAG_ACK,1001,0);
    } else {
        packet(s,TCP_FLAG_ACK,1000,0);
        assert(s->state==TCP_STATE_FIN_WAIT_2 && s->data.sndbuf_cur_sz==0);
        packet(s,TCP_FLAG_FIN|TCP_FLAG_ACK,1000,0);
    }
    assert(s->state==TCP_STATE_TIME_WAIT && !s->data.sndbuf && !s->data.rcvbuf);
    tick(2*TCP_DEFAULT_MSL-1);
    unsigned sent=acks;
    packet(s,TCP_FLAG_FIN|TCP_FLAG_ACK,1000,0);
    assert(acks==sent+1 && last_ack==1001 && s->data.timer==now);
    tick(2*TCP_DEFAULT_MSL-1);
    assert(!LIST_EMPTY(&tcp_socks));
    tick(1);
    assert(LIST_EMPTY(&tcp_socks));
}
static void greeting(bool blocking) {
    struct tcp_sock *s = connection(TCP_STATE_SYN_RECEIVED,51);
    s->data.snd.una=50;
    net_socket_t h={s};
    if(blocking) s->flags=0;
    assert(!(net_tcp_poll(&h,POLLOUT)&POLLOUT));
    const char *msg="220 Ready\r\n";
    unsigned n=strlen(msg),before=data_packets;
    ssize_t queued=net_tcp_sendto(&h,msg,n,0,NULL,0);
    if(blocking) assert(queued==(ssize_t)n);
    else assert(queued==-1 && errno==EWOULDBLOCK);
    assert(data_packets==before && s->data.snd.nxt==51);
    packet(s,TCP_FLAG_ACK,1000,51);
    assert(s->state==TCP_STATE_ESTABLISHED && (net_tcp_poll(&h,POLLOUT)&POLLOUT));
    if(!blocking) assert(net_tcp_sendto(&h,msg,n,0,NULL,0)==(ssize_t)n);
    assert(data_packets==before+1 && last_seq==51 && last_data_size==n);
    descriptor_close(s);
    assert(s->data.sndbuf_cur_sz==n && s->state==TCP_STATE_ESTABLISHED);
    packet(s,TCP_FLAG_ACK|TCP_FLAG_FIN,1000,51+n);
    assert(s->data.sndbuf_cur_sz==0 && s->state==TCP_STATE_CLOSE_WAIT);
    tick(10);
    assert(s->state==TCP_STATE_LAST_ACK && last_seq==51+n);
    packet(s,TCP_FLAG_ACK,1001,52+n);
    tick(10); assert(LIST_EMPTY(&tcp_socks));
}

int main(void) {
    /* Every truncation of a valid MSS-bearing SYN must be rejected before
     * the packet state machine reads the missing fields. */
    uint8_t header[24]={0};
    tcp_hdr_t *tcp=(tcp_hdr_t *)header;
    tcp->off_flags=htons(TCP_OFFSET(6)|TCP_FLAG_SYN);
    header[20]=TCP_OPT_MSS; header[21]=4; header[22]=5; header[23]=180;
    for(unsigned n=0;n<sizeof(header);++n) assert(!tcp_header_valid(header,n));
    assert(tcp_header_valid(header,sizeof(header)));
    tcp->off_flags=htons(TCP_OFFSET(5)|TCP_FLAG_ACK);
    assert(tcp_header_valid(header,sizeof(tcp_hdr_t)));
    passive(); active(false); active(true); greeting(false); greeting(true);
    struct tcp_sock *s=connection(TCP_STATE_ESTABLISHED,42);
    descriptor_close(s);
    tick(120000);
    assert(LIST_EMPTY(&tcp_socks));
    /* More requests than fit into console RAM if TIME_WAIT pins payload buffers. */
    for(unsigned i=0;i<150;++i) {
        s=connection(TCP_STATE_ESTABLISHED,42);
        descriptor_close(s);
        packet(s,TCP_FLAG_FIN|TCP_FLAG_ACK,1000,43);
        assert(s->state==TCP_STATE_TIME_WAIT && !s->data.sndbuf && !s->data.rcvbuf);
    }
    tick(2*TCP_DEFAULT_MSL); assert(LIST_EMPTY(&tcp_socks));
    puts("Patched KOS TCP handshake, passive/active/simultaneous close, FIN retry, orphan expiry and 150 TIME_WAIT sockets passed");
}
