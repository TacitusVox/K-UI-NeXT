/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/preview_prefix.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Metadata-only fixture. CRC and compressed frame validation happen in the
 * decoder. Model two sequential streams and arbitrary TLS record boundaries. */
static size_t page(uint8_t *p, uint8_t sid, uint8_t seq, unsigned count, unsigned bytes, bool bos) {
    memset(p, 0, 27+count*(1+bytes));
    memcpy(p, "OggS", 4); p[5]=bos?2:0; p[14]=sid; p[18]=seq; p[26]=(uint8_t)count;
    memset(p+27, (int)bytes, count);
    return 27+count*(1+bytes);
}
static size_t process(const uint8_t *b, size_t len, size_t chunk, bool *complete) {
    struct kui_preview_prefix p = {0};
    size_t at=0;
    while(at<len && !p.complete) {
        size_t got=len-at<chunk?len-at:chunk;
        size_t keep=kui_preview_prefix_consume(&p,b+at,got);
        assert(keep==got || p.complete);
        at+=keep;
    }
    *complete=p.complete;
    return at;
}
int main(int argc, char **argv) {
    bool done;
    uint8_t stream[1024];
    size_t n=page(stream,3,0,1,8,true);
    size_t start=n;
    n+=page(stream+n,5,0,1,42,true);
    uint8_t *ident=stream+start+28;
    memcpy(ident,"\x80theora",7);
    ident[25]=3; ident[29]=2; /* 1.5 fps x 10 seconds = 15 packets. */
    n+=page(stream+n,3,1,12,2,false);
    n+=page(stream+n,5,1,2,5,false);
    n+=page(stream+n,5,2,12,0,false);
    size_t stop=n+page(stream+n,5,3,3,1,false);
    size_t full=stop+page(stream+stop,5,4,10,1,false);
    for(size_t chunk=1;chunk<=full+2;++chunk) {
        assert(process(stream,full,chunk,&done)==stop && done);
        assert(process(stream,stop-1,chunk,&done)==stop-1 && !done);
    }
    stream[n+18]=6; /* Sequence gap disables the optional shortcut. */
    assert(process(stream,full,7,&done)==full && !done);
    stream[0]='x';
    assert(process(stream,full,1,&done)==full && !done);
    /* Theora can reach 10 seconds before the next interleaved audio page.
     * Stop exactly at the first complete Vorbis page covering that interval. */
    uint8_t mixed[1024];
    size_t video=page(mixed,5,0,1,42,true);
    memcpy(mixed+28,"\x80theora",7); mixed[28+25]=1; mixed[28+29]=1;
    size_t audio_header=page(mixed+video,8,0,1,30,true);
    uint8_t *id=mixed+video+28;
    memcpy(id,"\1vorbis",7); id[11]=2; id[12]=0x80; id[13]=0x3e; /* 16 kHz */
    size_t music=video+audio_header;
    music+=page(mixed+music,5,1,12,1,false); /* Two remaining headers and ten one-second frames. */
    size_t early=music;
    music+=page(mixed+music,8,1,1,1,false);
    memset(mixed+early+6,0xff,8); /* no completed granule yet */
    size_t late=music;
    music+=page(mixed+music,8,2,1,1,false);
    mixed[late+6]=0; mixed[late+7]=0x71; mixed[late+8]=2; /* 160000 */
    size_t extra=page(mixed+music,5,2,1,1,false);
    for(size_t chunk=1;chunk<music+extra;++chunk) {
        assert(process(mixed,music+extra,chunk,&done)==music && done);
        assert(process(mixed,music-1,chunk,&done)==music-1 && !done);
    }
    mixed[early+5]=4; /* Short sound reaches EOS before ten seconds. */
    assert(process(mixed,music+extra,1,&done)==late && done);
    if(argc==3 || argc==4) {
        FILE *src=fopen(argv[1],"rb"),*dst=fopen(argv[2],"wb");
        assert(src && dst);
        struct kui_preview_prefix p={0};
        if(argc == 4) {
            unsigned start = (unsigned)atoi(argv[3]);
            assert(start <= KUI_PREVIEW_START_MAX_SECONDS);
            p.start_seconds = (uint8_t)start;
        }
        size_t used=0,read;
        uint8_t buf[4096];
        while(!p.complete && (read=fread(buf,1,sizeof(buf),src))) {
            size_t keep=kui_preview_prefix_consume(&p,buf,read);
            assert(fwrite(buf,1,keep,dst)==keep);
            used+=keep;
        }
        assert(p.complete && !fclose(src) && !fclose(dst));
        printf("Kept %zu compressed bytes for ten seconds\n",used);
    }
    puts("Ogg prefix, multiplex, fractional frame rate, chunk boundary and fallback checks passed");
    return 0;
}
