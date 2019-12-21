#pragma once
/*#
    # m6561.h

    MOS Technology 6561 emulator (aka VIC for PAL)

    Do this:
    ~~~C
    #define CHIPS_IMPL
    ~~~
    before you include this file in *one* C or C++ file to create the 
    implementation.

    Optionally provide the following macros with your own implementation
    ~~~C    
    CHIPS_ASSERT(c)
    ~~~

    ## Emulated Pins
    TODO

    TODO: Documentation

    ## zlib/libpng license

    Copyright (c) 2019 Andre Weissflog
    This software is provided 'as-is', without any express or implied warranty.
    In no event will the authors be held liable for any damages arising from the
    use of this software.
    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:
        1. The origin of this software must not be misrepresented; you must not
        claim that you wrote the original software. If you use this software in a
        product, an acknowledgment in the product documentation would be
        appreciated but is not required.
        2. Altered source versions must be plainly marked as such, and must not
        be misrepresented as being the original software.
        3. This notice may not be removed or altered from any source
        distribution. 
#*/
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* address pins (see control pins for chip-select condition) */
#define M6561_A0    (1ULL<<0)
#define M6561_A1    (1ULL<<1)
#define M6561_A2    (1ULL<<2)
#define M6561_A3    (1ULL<<3)
#define M6561_A4    (1ULL<<4)
#define M6561_A5    (1ULL<<5)
#define M6561_A6    (1ULL<<6)
#define M6561_A7    (1ULL<<7)
#define M6561_A8    (1ULL<<8)
#define M6561_A9    (1ULL<<9)
#define M6561_A10   (1ULL<<10)
#define M6561_A11   (1ULL<<11)
#define M6561_A12   (1ULL<<12)
#define M6561_A13   (1ULL<<13)

/* data pins (the 4 additional color data pins are not emulated as pins) */
#define M6561_D0    (1ULL<<16)
#define M6561_D1    (1ULL<<17)
#define M6561_D2    (1ULL<<18)
#define M6561_D3    (1ULL<<19)
#define M6561_D4    (1ULL<<20)
#define M6561_D5    (1ULL<<21)
#define M6561_D6    (1ULL<<22)
#define M6561_D7    (1ULL<<23)

/* control pins shared with CPU 
    NOTE that there is no dedicated chip-select pin, instead
    registers are read and written during the CPU's 'shared bus
    half-cycle', and the following address bus pin mask is
    active:

    A13 A12 A11 A10 A9 A8
      0   1   0   0  0  0

    When this is true, the pins A3..A0 select the chip register.

    Use the M6561_SELECTED() macro to decide whether to call
    m6561_iorq()
*/
#define M6561_RW    (1ULL<<24)      /* same as M6502_RW */

#define M6561_CS_MASK           (M6561_A13|M6561_A12|M6561_A11|M6561_A10|M6561_A9|M6561_A8)
#define M6561_CS_VALUE          (M6561_A12)
#define M6561_SELECTED(pins)    ((pins&M6561_CS_MASK)==M6561_CS_VALUE)

#define M6561_NUM_REGS (16)
#define M6561_REG_MASK (M6561_NUM_REGS-1)

/* extract 8-bit data bus from 64-bit pins */
#define M6561_GET_DATA(p) ((uint8_t)((p&0xFF0000ULL)>>16))
/* merge 8-bit data bus value into 64-bit pins */
#define M6561_SET_DATA(p,d) {p=(((p)&~0xFF0000ULL)|(((d)<<16)&0xFF0000ULL));}

/* memory fetch callback, used to feed pixel- and color-data into the m6561 */
typedef uint16_t (*m6561_fetch_t)(uint16_t addr, void* user_data);

/* setup parameters for m6561_init() function */
typedef struct {
    /* pointer to RGBA8 framebuffer for generated image (optional) */
    uint32_t* rgba8_buffer;
    /* size of the RGBA framebuffer (optional) */
    uint32_t rgba8_buffer_size;
    /* visible CRT area blitted to rgba8_buffer (in pixels) */
    uint16_t vis_x, vis_y, vis_w, vis_h;
    /* the memory-fetch callback */
    m6561_fetch_t fetch_cb;
    /* optional user-data for fetch callback */
    void* user_data;
} m6561_desc_t;

/* raster unit state */
typedef struct {
    uint8_t h_count;        /* horizontal tick counter */
    uint16_t v_count;       /* line counter */
    uint16_t vc;            /* video matrix counter */
    uint16_t vc_base;       /* vc reload value at start of line */
    uint8_t vc_disabled;    /* if != 0, video counter inactive */
    uint8_t rc;             /* 4-bit raster counter (0..7 or 0..15) */
    uint8_t row_height;     /* either 8 or 16 */
    uint8_t row_count;      /* character row count */
} m6561_raster_unit_t;

/* memory unit  state */
typedef struct {
    uint16_t c_addr_base;   /* character access base address */
    uint16_t g_addr_base;   /* graphics access base address */
    uint16_t c_value;       /* last fetched character access value */
    m6561_fetch_t fetch_cb; /* memory fetch callback */
    void* user_data;        /* memory fetch callback user data */
} m6561_memory_unit_t;

/* graphics unit state */
typedef struct {
    uint8_t shift;          /* current pixel shifter */
    uint8_t color;          /* last fetched color value */
    uint32_t bg_color;      /* cached RGBA background color */
} m6561_graphics_unit_t;

/* border unit state */
typedef struct {
    uint8_t left, right;
    uint16_t top, bottom;
    uint32_t color;         /* cached RGBA border color */
    uint8_t enabled;        /* if != 0, in border area */
} m6561_border_unit_t;

/* CRT state tracking */
typedef struct {
    uint16_t x, y;              /* beam pos */
    uint16_t vis_x0, vis_y0, vis_x1, vis_y1;  /* the visible area */
    uint16_t vis_w, vis_h;      /* width of visible area */
    uint32_t* rgba8_buffer;
} m6561_crt_t;

/* sound generator state */
typedef struct {
    float sample;
} m6561_sound_t;

/* the m6561_t state struct */
typedef struct {
    // FIXME
    bool debug_vis;
    uint8_t regs[M6561_NUM_REGS];
    m6561_raster_unit_t rs;
    m6561_memory_unit_t mem;
    m6561_border_unit_t border;
    m6561_graphics_unit_t gunit;
    m6561_crt_t crt;
    m6561_sound_t sound;
    uint64_t pins;
} m6561_t;

/* initialize a new m6561_t instance */
void m6561_init(m6561_t* vic, const m6561_desc_t* desc);
/* reset a m6561_t instance */
void m6561_reset(m6561_t* vic);
/* get the visible display width in pixels */
int m6561_display_width(m6561_t* vic);
/* get the visible display height in pixels */
int m6561_display_height(m6561_t* vic);
/* read/write m6561 registers */
uint64_t m6561_iorq(m6561_t* vic, uint64_t pins);
/* tick the m6561 instance, returns true when sound sample ready */
bool m6561_tick(m6561_t* vic);
/* get 32-bit RGBA8 value from color index (0..15) */
uint32_t m6561_color(int i);

#ifdef __cplusplus
} /* extern "C" */
#endif

/*--- IMPLEMENTATION ---------------------------------------------------------*/
#ifdef CHIPS_IMPL
#include <string.h>
#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

/* internal constants */
#define _M6561_HTOTAL       (71)
#define _M6561_VTOTAL       (312)
#define _M6561_VRETRACEPOS  (303)
#define _M6561_PIXELS_PER_TICK  (4)

#define _M6561_HBORDER (1<<0)
#define _M6561_VBORDER (1<<1)
#define _M6561_HVC_DISABLE (1<<0)
#define _M6561_VVC_DISABLE (1<<1)

#define _M6561_RGBA8(r,g,b) (0xFF000000|(b<<16)|(g<<8)|(r))

/* this palette taken from MAME */
static const uint32_t _m6561_colors[16] = {
    _M6561_RGBA8(0x00, 0x00, 0x00),
    _M6561_RGBA8(0xff, 0xff, 0xff),
    _M6561_RGBA8(0xf0, 0x00, 0x00),
    _M6561_RGBA8(0x00, 0xf0, 0xf0),
    _M6561_RGBA8(0x60, 0x00, 0x60),
    _M6561_RGBA8(0x00, 0xa0, 0x00),
    _M6561_RGBA8(0x00, 0x00, 0xf0),
    _M6561_RGBA8(0xd0, 0xd0, 0x00),
    _M6561_RGBA8(0xc0, 0xa0, 0x00),
    _M6561_RGBA8(0xff, 0xa0, 0x00),
    _M6561_RGBA8(0xf0, 0x80, 0x80),
    _M6561_RGBA8(0x00, 0xff, 0xff),
    _M6561_RGBA8(0xff, 0x00, 0xff),
    _M6561_RGBA8(0x00, 0xff, 0x00),
    _M6561_RGBA8(0x00, 0xa0, 0xff),
    _M6561_RGBA8(0xff, 0xff, 0x00)
};

static void _m6561_init_crt(m6561_crt_t* crt, const m6561_desc_t* desc) {
    /* vis area horizontal coords must be multiple of 8 */
    CHIPS_ASSERT((desc->vis_x & 7) == 0);
    CHIPS_ASSERT((desc->vis_w & 7) == 0);
    crt->rgba8_buffer = desc->rgba8_buffer;
    crt->vis_x0 = desc->vis_x/_M6561_PIXELS_PER_TICK;
    crt->vis_y0 = desc->vis_y;
    crt->vis_w = desc->vis_w/_M6561_PIXELS_PER_TICK;
    crt->vis_h = desc->vis_h;
    crt->vis_x1 = crt->vis_x0 + crt->vis_w;
    crt->vis_y1 = crt->vis_y0 + crt->vis_h;
}

void m6561_init(m6561_t* vic, const m6561_desc_t* desc) {
    CHIPS_ASSERT(vic && desc && desc->fetch_cb);
    CHIPS_ASSERT((0 == desc->rgba8_buffer) || (desc->rgba8_buffer_size >= (_M6561_HTOTAL*8*_M6561_VTOTAL*sizeof(uint32_t))));
    memset(vic, 0, sizeof(*vic));
    _m6561_init_crt(&vic->crt, desc);
    vic->border.enabled = _M6561_HBORDER|_M6561_VBORDER;
    vic->mem.fetch_cb = desc->fetch_cb;
    vic->mem.user_data = desc->user_data;
}

static void _m6561_reset_crt(m6561_crt_t* c) {
    c->x = c->y = 0;
}

static void _m6561_reset_raster_unit(m6561_t* vic) {
    memset(&vic->rs, 0, sizeof(vic->rs));
}

static void _m6561_reset_border_unit(m6561_t* vic) {
    memset(&vic->border, 0, sizeof(vic->border));
    vic->border.enabled = _M6561_HBORDER|_M6561_VBORDER;
}

static void _m6561_reset_memory_unit(m6561_t* vic) {
    memset(&vic->mem, 0, sizeof(vic->mem));
}

static void _m6561_reset_graphics_unit(m6561_t* vic) {
    memset(&vic->gunit, 0, sizeof(vic->gunit));
}

void m6561_reset(m6561_t* vic) {
    CHIPS_ASSERT(vic);
    memset(&vic->regs, 0, sizeof(vic->regs));
    _m6561_reset_raster_unit(vic);
    _m6561_reset_border_unit(vic);
    _m6561_reset_memory_unit(vic);
    _m6561_reset_graphics_unit(vic);
    _m6561_reset_crt(&vic->crt);
    // FIXME: reset audio unit
}

int m6561_display_width(m6561_t* vic) {
    CHIPS_ASSERT(vic);
    return _M6561_PIXELS_PER_TICK * (vic->debug_vis ? _M6561_HTOTAL : vic->crt.vis_w);
}

int m6561_display_height(m6561_t* vic) {
    CHIPS_ASSERT(vic);
    return vic->debug_vis ? _M6561_VTOTAL : vic->crt.vis_h;
}

uint32_t m6561_color(int i) {
    CHIPS_ASSERT((i >= 0) && (i < 16));
    return _m6561_colors[i];
}

/* update precomputed values when disp-related registers changed */
static void _m6561_regs_dirty(m6561_t* vic) {
    /* each column is 2 ticks */
    vic->rs.row_height = (vic->regs[3] & 1) ? 16 : 8;
    vic->border.left = vic->regs[0] & 0x7F;
    vic->border.right = vic->border.left + (vic->regs[2] & 0x7F) * 2;
    vic->border.top = vic->regs[1];
    vic->border.bottom = vic->border.top + ((vic->regs[3]>>1) & 0x3F) * vic->rs.row_height;
    vic->border.color = _m6561_colors[vic->regs[15] & 7];
    vic->gunit.bg_color = _m6561_colors[(vic->regs[15]>>4) & 0xF];
    vic->mem.g_addr_base = ((vic->regs[5] & 0xF)<<10);  // A13..A10
    vic->mem.c_addr_base = (((vic->regs[5]>>4)&0xF)<<10) | // A13..A10
                           (((vic->regs[2]>>7)&1)<<9);    // A9
}

uint64_t m6561_iorq(m6561_t* vic, uint64_t pins) {
    CHIPS_ASSERT(vic);
    uint8_t addr = pins & M6561_REG_MASK;
    if (pins & M6561_RW) {
        /* read */
        uint8_t data;
        switch (addr) {
            case 3:
                data = ((vic->rs.v_count & 1)<<7) | (vic->regs[addr] & 0x7F);
                break;
            case 4:
                data = (vic->rs.v_count>>1) & 0xFF;
                break;
            /* not implemented: light pen and potentiometers */
            default:
                data = vic->regs[addr];
                break;
        }
        M6561_SET_DATA(pins, data);
    }
    else {
        /* write */
        const uint8_t data = M6561_GET_DATA(pins);
        vic->regs[addr] = data;
        _m6561_regs_dirty(vic);
    }
    return pins;
}

static inline void _m6561_crt_next_scanline(m6561_t* vic) {
    vic->crt.x = 0;
    if (vic->rs.v_count == _M6561_VRETRACEPOS) {
        vic->crt.y = 0;
    }
    else {
        vic->crt.y++;
    }
}

bool m6561_tick(m6561_t* vic) {

    /* decode pixels, each tick is 4 pixels */
    if (vic->crt.rgba8_buffer) {
        int x, y, w;
        if (vic->debug_vis) {
            x = vic->rs.h_count;
            y = vic->rs.v_count;
            w = _M6561_HTOTAL;
            uint32_t* dst = vic->crt.rgba8_buffer + (y * w + x) * _M6561_PIXELS_PER_TICK;
            // FIXME
            for (int i = 0; i < _M6561_PIXELS_PER_TICK; i++) {
                *dst++ = 0xFF00FF00;
            }
        }
        else if ((vic->crt.x >= vic->crt.vis_x0) && (vic->crt.x < vic->crt.vis_x1) &&
                 (vic->crt.y >= vic->crt.vis_y0) && (vic->crt.y < vic->crt.vis_y1))
        {
            const int x = vic->crt.x - vic->crt.vis_x0;
            const int y = vic->crt.y - vic->crt.vis_y0;
            const int w = vic->crt.vis_w;
            uint32_t* dst = vic->crt.rgba8_buffer + (y * w + x) * _M6561_PIXELS_PER_TICK;
            if (vic->border.enabled) {
                /* border area */
                uint32_t border_color = vic->border.color;
                *dst++ = border_color;
                *dst++ = border_color;
                *dst++ = border_color;
                *dst++ = border_color;
            }
            else {
                /* upper or lower nibble of last fetch pixel data */
                uint8_t p = vic->gunit.shift;
                uint32_t bg = vic->gunit.bg_color;
                uint32_t fg = _m6561_colors[vic->gunit.color & 7];
                *dst++ = (p & (1<<7)) ? fg : bg;
                *dst++ = (p & (1<<6)) ? fg : bg;
                *dst++ = (p & (1<<5)) ? fg : bg;
                *dst++ = (p & (1<<4)) ? fg : bg;
                vic->gunit.shift = p<<4;
            }
        }
    }

    /* display-enabled area? */
    if (vic->rs.h_count == vic->border.left) {
        /* enable fetching, but border still active for 1 tick */
        vic->rs.vc_disabled &= ~_M6561_HVC_DISABLE;
        vic->rs.vc = vic->rs.vc_base;
    }
    if (vic->rs.h_count == (vic->border.left+1)) {
        /* switch off horizontal border */
        vic->border.enabled &= ~_M6561_HBORDER;
    }
    if (vic->rs.h_count == (vic->border.right+1)) {
        /* switch on horizontal border */
        vic->border.enabled |= _M6561_HBORDER;
        vic->rs.vc_disabled |= _M6561_HVC_DISABLE;
    }

    /* fetch data */
    if (!vic->rs.vc_disabled) {
        if (vic->rs.vc & 1) {
            /* a g-access (graphics data) into pixel shifter */
            uint16_t addr = vic->mem.g_addr_base |
                            ((vic->mem.c_value & 0xFF) * vic->rs.row_height) |
                            vic->rs.rc;
            vic->gunit.shift = (uint8_t) vic->mem.fetch_cb(addr, vic->mem.user_data);
            vic->gunit.color = (vic->mem.c_value>>8) & 0xF;
        }
        else {
            /* a c-access (character code and color) */
            uint16_t addr = vic->mem.c_addr_base | (vic->rs.vc>>1);
            vic->mem.c_value = vic->mem.fetch_cb(addr, vic->mem.user_data);
        }
        vic->rs.vc = (vic->rs.vc + 1) & ((1<<11)-1);
    }

    /* tick horizontal and vertical counters */
    vic->rs.h_count++;
    vic->crt.x++;
    if (vic->rs.h_count == _M6561_HTOTAL) {
        vic->rs.h_count = 0;
        vic->rs.v_count++;
        vic->rs.rc++;
        if (vic->rs.rc == vic->rs.row_height) {
            vic->rs.rc = 0;
            vic->rs.row_count++;
            vic->rs.vc_base = vic->rs.vc & 0xFFFE;
        }
        _m6561_crt_next_scanline(vic);
        if (vic->rs.v_count == vic->border.top) {
            vic->border.enabled &= ~_M6561_VBORDER;
            vic->rs.vc_disabled &= ~_M6561_VVC_DISABLE;
            vic->rs.vc = 0;
            vic->rs.vc_base = 0;
            vic->rs.row_count = 0;
            vic->rs.rc = 0;
        }
        if (vic->rs.v_count == vic->border.bottom) {
            vic->border.enabled |= _M6561_VBORDER;
            vic->rs.vc_disabled |= _M6561_VVC_DISABLE;
        }
        if (vic->rs.v_count == _M6561_VTOTAL) {
            vic->rs.v_count = 0;
        }
    }

    /* FIXME: sound generation */

    return false;
}

#endif