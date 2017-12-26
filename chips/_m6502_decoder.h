/* machine generated, don't edit! */
/* set 16-bit address in 64-bit pin mask*/
#define _SA(addr) pins=(pins&~0xFFFF)|((addr)&0xFFFFULL)
/* set 16-bit address and 8-bit data in 64-bit pin mask */
#define _SAD(addr,data) pins=(pins&~0xFFFFFF)|(((data)<<16)&0xFF0000ULL)|((addr)&0xFFFFULL)
/* extract 8-bit data from 64-bit pin mask */
#define _GD() ((uint8_t)((pins&0xFF0000ULL)>>16))
/* enable control pins */
#define _ON(m) pins|=(m)
/* disable control pins */
#define _OFF(m) pins&=~(m)
/* execute a tick */
#define _T() pins=tick(pins);ticks++
/* a memory read tick */
#define _RD() _ON(M6502_RW);_T();
/* a memory write tick */
#define _WR() _OFF(M6502_RW);_T()
uint32_t m6502_exec(m6502_t* cpu, uint32_t num_ticks) {
  m6502_t c = *cpu;
  uint8_t l, h;
  uint32_t ticks = 0;
  uint64_t pins = c.PINS;
  const m6502_tick_t tick = c.tick;
  do {
    /* fetch opcode */
    _SA(c.PC++);_ON(M6502_SYNC);_RD();_OFF(M6502_SYNC);
    const uint8_t opcode = _GD();
    switch (opcode) {
    }
    /* check for interrupt request */
    if ((pins & M6502_NMI) || ((pins & M6502_IRQ) && !(c.P & M6502_IF))) {
      /* execute a slightly modified BRK instruction */
      _RD();
      _SAD(0x0100|c.S--, c.PC>>8); _WR();
      _SAD(0x0100|c.S--, c.PC); _WR();
      _SAD(0x0100|c.S--, c.P&~M6502_BF); _WR();
      if (pins & M6502_NMI) {
        _SA(0xFFFA); _RD(); l=_GD();
        _SA(0xFFFB); _RD(); h=_GD();
      }
      else {
        _SA(0xFFFE); _RD(); l=_GD();
        _SA(0xFFFF); _RD(); h=_GD();
      }
      c.PC = (h<<8)|l;
      c.P |= M6502_IF;
      pins &= ~(M6502_IRQ|M6502_NMI);
    }
  } while ((ticks < num_ticks) && ((pins & c.break_mask)==0));
  c.PINS = pins;
  *cpu = c;
  return ticks;
}
#undef _SA
#undef _SAD
#undef _GD
#undef _ON
#undef _OFF
#undef _T
#undef _RD
#undef _WR