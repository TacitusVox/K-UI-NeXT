/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/shell_font.h"
#include <limits.h>
#include <stddef.h>

struct glyph { uint16_t offset;uint8_t width,height,stride,advance,y; };
#include "shell_font_data.inc"

/* The bootstrap and freestanding game helpers keep their original ASCII
 * object. The GUI and its host renderer also have fixed Japanese masks. */
#if !defined(__sh__) || defined(KUI_GUI_JAPANESE)
#define WITH_JAPANESE_GLYPHS 1
#include "kui/japanese_text.h"
#include "shell_font_ja_data.inc"

struct japanese_glyph {
    const uint16_t *rows;
    unsigned width;
};

static struct japanese_glyph japanese_glyph(uint32_t scalar) {
    if(scalar>=0xff61 && scalar<=0xff9f)
        return (struct japanese_glyph){japanese_half_rows+(scalar-0xff61)*KUI_SHELL_FONT_SMALL_HEIGHT,8};
    size_t low=0,high=sizeof(japanese_scalars)/sizeof(japanese_scalars[0]);
    while(low<high) {
        size_t mid=low+(high-low)/2;
        if(japanese_scalars[mid]<scalar) low=mid+1;
        else if(japanese_scalars[mid]>scalar) high=mid;
        else return (struct japanese_glyph){japanese_wide_rows+mid*KUI_SHELL_FONT_SMALL_HEIGHT,16};
    }
    return (struct japanese_glyph){NULL,0};
}
#endif

static const struct glyph *glyph_for(unsigned char code,bool large) {
    if(code<32 || code>126) code='?';
    if(large) return &large_glyphs[code-32];
    return &small_glyphs[code-32];
}
#ifdef WITH_JAPANESE_GLYPHS
unsigned kui_shell_font_advance(uint32_t scalar,bool large) {
    if(scalar>=32 && scalar<=126) return glyph_for((unsigned char)scalar,large)->advance;
    unsigned width=japanese_glyph(scalar).width;
    return large?width+width/2:width;
}
#endif
static unsigned font_width(const char *text,bool large,unsigned spacing) {
    if(!text) return 0;
    unsigned width=0;
    for(const unsigned char *p=(const unsigned char *)text;*p;p++) {
        unsigned step=glyph_for(*p,large)->advance;
        const unsigned char *current=p;
#ifdef WITH_JAPANESE_GLYPHS
        if(*p>=128) {
            uint32_t scalar;
            p+=kui_utf8_next((const char *)p,&scalar)-1;
            unsigned advance=kui_shell_font_advance(scalar,large);
            if(advance) step=advance;
        }
#endif
        if(current!=(const unsigned char *)text) step+=spacing;
        if(width>UINT_MAX-step) return UINT_MAX;
        width+=step;
    }
    return width;
}
unsigned kui_shell_font_width(const char *text,bool large) {
    return font_width(text,large,KUI_SHELL_FONT_LETTER_SPACING);
}
unsigned kui_shell_font_width_crisp(const char *text,bool large) {
    return font_width(text,large,0);
}
static uint16_t blend(uint16_t background,uint16_t color,unsigned alpha) {
    unsigned inverse=15-alpha;
    unsigned r=(((color>>11)&31u)*alpha+((background>>11)&31u)*inverse+7u)/15u;
    unsigned g=(((color>>5)&63u)*alpha+((background>>5)&63u)*inverse+7u)/15u;
    unsigned b=((color&31u)*alpha+(background&31u)*inverse+7u)/15u;
    return (uint16_t)((r<<11)|(g<<5)|b);
}
static void font_draw(uint16_t *frame,int x,int y,uint16_t color,
                      const char *text,bool large,bool crisp,int clip_top,int clip_bottom) {
    if(clip_top<0) clip_top=0;
    if(clip_top>480) clip_top=480;
    if(clip_bottom<0) clip_bottom=0;
    if(clip_bottom>480) clip_bottom=480;
    unsigned height=large?KUI_SHELL_FONT_LARGE_HEIGHT:KUI_SHELL_FONT_SMALL_HEIGHT;
    if(clip_top>=clip_bottom || !frame || !text || x>=640 ||
       y>=clip_bottom || (int64_t)y+height<=clip_top) return;
    const uint8_t *pixels=large?large_pixels:small_pixels;
    int64_t pen=x;
    for(const unsigned char *p=(const unsigned char *)text;*p && pen<640;p++) {
        unsigned char code=*p;
#ifdef WITH_JAPANESE_GLYPHS
        if(code>=128) {
            uint32_t scalar;
            p+=kui_utf8_next((const char *)p,&scalar)-1;
            struct japanese_glyph jp=japanese_glyph(scalar);
            if(jp.rows) {
                unsigned width=large?jp.width+jp.width/2:jp.width;
                if(pen+width>0) {
                    unsigned first=pen<0?(unsigned)-pen:0;
                    unsigned last=pen+width>640?(unsigned)(640-pen):width;
                    for(unsigned row=0;row<height;row++) {
                        int64_t dy=(int64_t)y+row;
                        if(dy<clip_top || dy>=clip_bottom) continue;
                        uint16_t bits=jp.rows[large?(row*3)/4:row];
                        for(unsigned col=first;col<last;col++) {
                            unsigned source=large?(col*2)/3:col;
                            if(bits & (1u<<(jp.width-1u-source)))
                                frame[(size_t)dy*640+(size_t)(pen+col)]=color;
                        }
                    }
                }
                pen+=width+(crisp?0:KUI_SHELL_FONT_LETTER_SPACING);
                continue;
            }
            code='?';
        }
#endif
        const struct glyph *g=glyph_for(code,large);
        if(pen+g->width>0) {
            unsigned first=pen<0?(unsigned)-pen:0;
            unsigned last=pen+g->width>640?(unsigned)(640-pen):g->width;
            for(unsigned row=0;row<g->height;row++) {
                int64_t dy=(int64_t)y+g->y+row;
                if(dy<clip_top || dy>=clip_bottom) continue;
                const uint8_t *mask=pixels+g->offset+row*g->stride;
                for(unsigned col=first;col<last;col++) {
                    unsigned alpha=(mask[col/2]>>((col&1)?0:4))&15u;
                    if(!alpha || (crisp && alpha<8u)) continue;
                    uint16_t *target=frame+(size_t)dy*640+(size_t)(pen+col);
                    *target=crisp || alpha==15?color:blend(*target,color,alpha);
                }
            }
        }
        pen+=g->advance+(crisp?0:KUI_SHELL_FONT_LETTER_SPACING);
    }
}
void kui_shell_font_draw(uint16_t *frame,int x,int y,uint16_t color,
                         const char *text,bool large) {
    font_draw(frame,x,y,color,text,large,false,0,480);
}
void kui_shell_font_draw_clipped(uint16_t *frame,int x,int y,uint16_t color,
                                 const char *text,bool large,int clip_top,int clip_bottom) {
    font_draw(frame,x,y,color,text,large,false,clip_top,clip_bottom);
}
void kui_shell_font_draw_crisp(uint16_t *frame,int x,int y,uint16_t color,
                               const char *text,bool large) {
    font_draw(frame,x,y,color,text,large,true,0,480);
}
