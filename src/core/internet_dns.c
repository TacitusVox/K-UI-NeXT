/* SPDX-License-Identifier: GPL-3.0-only */
#include "internet_internal.h"
#include <string.h>
#include <strings.h>

static unsigned word(const uint8_t *p) { return (unsigned)p[0] << 8 | p[1]; }
size_t kui_dns_query(uint8_t *p, size_t cap, uint16_t id, const char *host) {
    size_t len=strlen(host);
    if(!len || len>253 || cap<len+18) return 0;
    memset(p,0,12);
    p[0]=id>>8; p[1]=(uint8_t)id; p[2]=1; p[5]=1;
    size_t at=12;
    while(*host) {
        size_t label=strcspn(host,".");
        if(!label || label>63) return 0;
        p[at++]=(uint8_t)label;
        memcpy(p+at,host,label); at+=label;
        host+=label;
        if(*host) ++host;
    }
    memcpy(p+at,"\0\0\1\0\1",5);
    return at+5;
}

/* Expand one bounded, possibly compressed DNS name. The caller advances by
 * the encoded length even when the name ends with a compression pointer. */
static bool name(const uint8_t *p,size_t len,size_t *at,char out[KUI_HTTP_HOST]) {
    size_t pos=*at, written=0;
    unsigned jumps=0;
    for(;;) {
        if(pos>=len) return false;
        unsigned n=p[pos++];
        if((n&0xc0)==0xc0) {
            if(pos>=len || ++jumps>24) return false;
            if(jumps==1) *at=pos+1;
            pos=((n&63u)<<8)|p[pos];
            continue;
        }
        if(n>63 || n>len-pos) return false;
        if(!n) {
            if(!jumps) *at=pos;
            out[written]=0;
            return true;
        }
        if(written+n+(written!=0)>=KUI_HTTP_HOST) return false;
        if(written) out[written++]='.';
        for(unsigned i=0;i<n;i++) {
            unsigned c=p[pos++];
            if(c<=32 || c>=127 || c=='.') return false;
            out[written++]=(char)c;
        }
    }
}
bool kui_dns_answer(const uint8_t *p,size_t len,uint16_t id,const char *host,uint8_t ip[4],uint32_t *ttl,
                    const uint8_t avoid[4]) {
    if(len<12 || word(p)!=id || (word(p+2)&0xfa0f)!=0x8000 || word(p+4)!=1) return false;
    size_t first=12;
    char owner[KUI_HTTP_HOST], wanted[KUI_HTTP_HOST];
    if(!name(p,len,&first,wanted) || strcasecmp(wanted,host) || len-first<4 ||
       word(p+first)!=1 || word(p+first+2)!=1) return false;
    first+=4;
    *ttl=UINT32_MAX;
    for(unsigned alias=0;alias<12;alias++) {
        size_t pos=first;
        bool next=false;
        const uint8_t *fallback=NULL;
        uint32_t fallback_ttl=0;
        for(unsigned i=0;i<word(p+6);i++) {
            if(!name(p,len,&pos,owner) || len-pos<10) return false;
            unsigned type=word(p+pos), cls=word(p+pos+2), bytes=word(p+pos+8);
            uint32_t age=((uint32_t)word(p+pos+4)<<16) | word(p+pos+6);
            pos+=10;
            if(bytes>len-pos) return false;
            if(cls==1 && !strcasecmp(owner,wanted)) {
                if(type==1 && bytes==4 && avoid && !memcmp(avoid,p+pos,4)) {
                    fallback=p+pos;
                    fallback_ttl=age;
                } else if(type==1 && bytes==4) {
                    memcpy(ip,p+pos,4);
                    if(age<*ttl) *ttl=age;
                    return true;
                }
                if(type==5) {
                    size_t end=pos;
                    if(!name(p,len,&end,wanted) || end!=pos+bytes) return false;
                    if(age<*ttl) *ttl=age;
                    next=true;
                    break;
                }
            }
            pos+=bytes;
        }
        if(!next && fallback) {
            memcpy(ip,fallback,4);
            if(fallback_ttl<*ttl) *ttl=fallback_ttl;
            return true;
        }
        if(!next) break;
    }
    return false;
}
