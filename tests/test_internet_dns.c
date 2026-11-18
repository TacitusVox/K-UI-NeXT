/* SPDX-License-Identifier: GPL-3.0-only */
#include "../src/core/internet_internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    uint8_t p[512],ip[4];uint32_t ttl=0;
    size_t n=kui_dns_query(p,sizeof(p),0x2345,"a.example.org");
    assert(n==31 && p[2]==1 && p[5]==1 && !memcmp(p+12,"\1a\7example\3org\0\0\1\0\1",19));
    p[2]=0x81;p[3]=0x80;p[7]=2;
    /* CNAME a -> b plus compressed A RR owner, TTL limited by the alias.
     * The separate owner check below rejects unrelated addresses. */
    static const uint8_t results[]={
        0xc0,0x0c,0,5,0,1,0,0,0,12,0,4,1,'b',0xc0,0x0e,
        0xc0,0x2b,0,1,0,1,0,0,0,90,0,4,192,0,2,44};
    memcpy(p+n,results,sizeof(results));size_t size=n+sizeof(results);
    assert(kui_dns_answer(p,size,0x2345,"A.EXAMPLE.ORG",ip,&ttl) && ttl==12 && ip[3]==44);
    assert(!kui_dns_answer(p,size,0x2344,"a.example.org",ip,&ttl));
    assert(!kui_dns_answer(p,size,0x2345,"b.example.org",ip,&ttl));
    assert(!kui_dns_answer(p,size-1,0x2345,"a.example.org",ip,&ttl));
    p[2]|=2;assert(!kui_dns_answer(p,size,0x2345,"a.example.org",ip,&ttl));p[2]&=(uint8_t)~2u;
    p[n+15]=0x2b;assert(!kui_dns_answer(p,size,0x2345,"a.example.org",ip,&ttl));p[n+15]=0x0e;
    /* A response for another owner is never selected from a recursive response. */
    p[n+16+1]=0x0e;assert(!kui_dns_answer(p,size,0x2345,"a.example.org",ip,&ttl));
    assert(!kui_dns_query(p,sizeof(p),1,"a..org") && !kui_dns_query(p,20,1,"example.org"));
    puts("DNS queries, compressed CNAME chains, TTL, identity and bounds passed");
}
