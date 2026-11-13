#!/usr/bin/env python3
"""sidref - run a PSID tune in a minimal 6502 emulator and print the SID
register state after every play call. Reference for checking the MEGA65
player (see README.md in this directory).

usage: sidref.py file.sid [song] [calls]

Output: one line per play call: call number, then 25 register bytes
($D400-$D418) in hex. Only legal NMOS 6502 opcodes; SID registers read
back as written (oscillator 3 / envelope 3 read 0).
"""
import struct
import sys


class CPU:
    def __init__(self, mem):
        self.m = mem
        self.a = self.x = self.y = 0
        self.sp = 0xFF
        self.p = 0x24
        self.pc = 0
        self.sid = [0] * 32
        self.writes = 0

    # memory: $D400-$D7FF is the SID (mirrored every 32 bytes)
    def rd(self, a):
        a &= 0xFFFF
        if 0xD400 <= a < 0xD800:
            r = a & 0x1F
            return 0 if r in (0x1B, 0x1C) else self.sid[r]
        return self.m[a]

    def wr(self, a, v):
        a &= 0xFFFF
        v &= 0xFF
        if 0xD400 <= a < 0xD800:
            self.sid[a & 0x1F] = v
            self.writes += 1
        else:
            self.m[a] = v

    def rd16(self, a):
        return self.rd(a) | (self.rd((a + 1) & 0xFFFF) << 8)

    def rd16zp(self, a):
        return self.rd(a & 0xFF) | (self.rd((a + 1) & 0xFF) << 8)

    def push(self, v):
        self.m[0x100 + self.sp] = v & 0xFF
        self.sp = (self.sp - 1) & 0xFF

    def pull(self):
        self.sp = (self.sp + 1) & 0xFF
        return self.m[0x100 + self.sp]

    def nz(self, v):
        v &= 0xFF
        self.p = (self.p & 0x7D) | (v & 0x80) | (0 if v else 0x02)
        return v

    def flag(self, bit, on):
        self.p = (self.p | bit) if on else (self.p & ~bit & 0xFF)

    # addressing: returns effective address
    def ea(self, mode):
        pc = self.pc
        if mode == 'imm':
            self.pc += 1
            return pc
        if mode == 'zp':
            self.pc += 1
            return self.rd(pc)
        if mode == 'zpx':
            self.pc += 1
            return (self.rd(pc) + self.x) & 0xFF
        if mode == 'zpy':
            self.pc += 1
            return (self.rd(pc) + self.y) & 0xFF
        if mode == 'abs':
            self.pc += 2
            return self.rd16(pc)
        if mode == 'abx':
            self.pc += 2
            return (self.rd16(pc) + self.x) & 0xFFFF
        if mode == 'aby':
            self.pc += 2
            return (self.rd16(pc) + self.y) & 0xFFFF
        if mode == 'izx':
            self.pc += 1
            return self.rd16zp(self.rd(pc) + self.x)
        if mode == 'izy':
            self.pc += 1
            return (self.rd16zp(self.rd(pc)) + self.y) & 0xFFFF
        if mode == 'ind':
            self.pc += 2
            p = self.rd16(pc)
            return self.rd(p) | (self.rd((p & 0xFF00) | ((p + 1) & 0xFF)) << 8)
        raise ValueError(mode)

    def adc(self, v):
        c = self.p & 1
        if self.p & 0x08:  # decimal
            lo = (self.a & 0x0F) + (v & 0x0F) + c
            hi = (self.a >> 4) + (v >> 4)
            if lo > 9:
                lo += 6
                hi += 1
            self.flag(0x40, (~(self.a ^ v) & (self.a ^ (hi << 4)) & 0x80))
            if hi > 9:
                hi += 6
            self.flag(0x01, hi > 15)
            self.a = self.nz(((hi << 4) | (lo & 0x0F)) & 0xFF)
        else:
            r = self.a + v + c
            self.flag(0x01, r > 0xFF)
            self.flag(0x40, (~(self.a ^ v) & (self.a ^ r) & 0x80))
            self.a = self.nz(r)

    def sbc(self, v):
        if self.p & 0x08:
            c = self.p & 1
            lo = (self.a & 0x0F) - (v & 0x0F) - (1 - c)
            hi = (self.a >> 4) - (v >> 4)
            if lo < 0:
                lo -= 6
                hi -= 1
            if hi < 0:
                hi -= 6
            r = self.a - v - (1 - c)
            self.flag(0x01, r >= 0)
            self.flag(0x40, ((self.a ^ v) & (self.a ^ r) & 0x80))
            self.a = self.nz(((hi << 4) | (lo & 0x0F)) & 0xFF)
        else:
            self.adc(v ^ 0xFF)

    def cmp(self, reg, v):
        r = reg - v
        self.flag(0x01, r >= 0)
        self.nz(r)

    def step(self):
        op = self.rd(self.pc)
        self.pc = (self.pc + 1) & 0xFFFF
        if op not in OPS:
            raise RuntimeError('illegal opcode $%02X at $%04X' % (op, self.pc - 1))
        name, mode = OPS[op]
        getattr(self, 'op_' + name)(mode)

    def val(self, mode):
        return self.rd(self.ea(mode))

    # ---- instructions
    def op_lda(self, m): self.a = self.nz(self.val(m))
    def op_ldx(self, m): self.x = self.nz(self.val(m))
    def op_ldy(self, m): self.y = self.nz(self.val(m))
    def op_sta(self, m): self.wr(self.ea(m), self.a)
    def op_stx(self, m): self.wr(self.ea(m), self.x)
    def op_sty(self, m): self.wr(self.ea(m), self.y)
    def op_tax(self, m): self.x = self.nz(self.a)
    def op_tay(self, m): self.y = self.nz(self.a)
    def op_txa(self, m): self.a = self.nz(self.x)
    def op_tya(self, m): self.a = self.nz(self.y)
    def op_tsx(self, m): self.x = self.nz(self.sp)
    def op_txs(self, m): self.sp = self.x
    def op_pha(self, m): self.push(self.a)
    def op_php(self, m): self.push(self.p | 0x30)
    def op_pla(self, m): self.a = self.nz(self.pull())
    def op_plp(self, m): self.p = (self.pull() & 0xEF) | 0x20
    def op_and(self, m): self.a = self.nz(self.a & self.val(m))
    def op_ora(self, m): self.a = self.nz(self.a | self.val(m))
    def op_eor(self, m): self.a = self.nz(self.a ^ self.val(m))
    def op_adc(self, m): self.adc(self.val(m))
    def op_sbc(self, m): self.sbc(self.val(m))
    def op_cmp(self, m): self.cmp(self.a, self.val(m))
    def op_cpx(self, m): self.cmp(self.x, self.val(m))
    def op_cpy(self, m): self.cmp(self.y, self.val(m))

    def op_bit(self, m):
        v = self.val(m)
        self.p = (self.p & 0x3D) | (v & 0xC0) | (0 if (self.a & v) else 0x02)

    def rmw(self, m, f):
        if m == 'acc':
            self.a = self.nz(f(self.a))
        else:
            a = self.ea(m)
            self.wr(a, self.nz(f(self.rd(a))))

    def op_inc(self, m): self.rmw(m, lambda v: v + 1)
    def op_dec(self, m): self.rmw(m, lambda v: v - 1)
    def op_inx(self, m): self.x = self.nz(self.x + 1)
    def op_iny(self, m): self.y = self.nz(self.y + 1)
    def op_dex(self, m): self.x = self.nz(self.x - 1)
    def op_dey(self, m): self.y = self.nz(self.y - 1)

    def op_asl(self, m):
        def f(v):
            self.flag(0x01, v & 0x80)
            return v << 1
        self.rmw(m, f)

    def op_lsr(self, m):
        def f(v):
            self.flag(0x01, v & 1)
            return v >> 1
        self.rmw(m, f)

    def op_rol(self, m):
        def f(v):
            c = self.p & 1
            self.flag(0x01, v & 0x80)
            return (v << 1) | c
        self.rmw(m, f)

    def op_ror(self, m):
        def f(v):
            c = self.p & 1
            self.flag(0x01, v & 1)
            return (v >> 1) | (c << 7)
        self.rmw(m, f)

    def op_jmp(self, m): self.pc = self.ea(m)

    def op_jsr(self, m):
        t = self.ea('abs')
        r = (self.pc - 1) & 0xFFFF
        self.push(r >> 8)
        self.push(r)
        self.pc = t

    def op_rts(self, m):
        lo = self.pull()
        self.pc = ((self.pull() << 8) | lo) + 1 & 0xFFFF

    def op_rti(self, m):
        self.p = (self.pull() & 0xEF) | 0x20
        lo = self.pull()
        self.pc = (self.pull() << 8) | lo

    def op_brk(self, m):
        raise RuntimeError('BRK at $%04X' % ((self.pc - 1) & 0xFFFF))

    def branch(self, cond):
        off = self.rd(self.pc)
        self.pc = (self.pc + 1) & 0xFFFF
        if cond:
            self.pc = (self.pc + (off - 256 if off & 0x80 else off)) & 0xFFFF

    def op_bpl(self, m): self.branch(not self.p & 0x80)
    def op_bmi(self, m): self.branch(self.p & 0x80)
    def op_bvc(self, m): self.branch(not self.p & 0x40)
    def op_bvs(self, m): self.branch(self.p & 0x40)
    def op_bcc(self, m): self.branch(not self.p & 0x01)
    def op_bcs(self, m): self.branch(self.p & 0x01)
    def op_bne(self, m): self.branch(not self.p & 0x02)
    def op_beq(self, m): self.branch(self.p & 0x02)
    def op_clc(self, m): self.flag(0x01, False)
    def op_sec(self, m): self.flag(0x01, True)
    def op_cli(self, m): self.flag(0x04, False)
    def op_sei(self, m): self.flag(0x04, True)
    def op_clv(self, m): self.flag(0x40, False)
    def op_cld(self, m): self.flag(0x08, False)
    def op_sed(self, m): self.flag(0x08, True)
    def op_nop(self, m): pass

    def call(self, addr, a=0, limit=5_000_000):
        """JSR addr with a return to a sentinel; run until it returns."""
        self.a, self.x, self.y = a, 0, 0
        self.push(0xFF)          # return address $FFFF -> pc $0000 after rts
        self.push(0xFE)          # (pushed $FFFE, rts adds 1)
        sentinel = 0xFFFF
        self.pc = addr
        n = 0
        while self.pc != sentinel:
            self.step()
            n += 1
            if n > limit:
                raise RuntimeError('call to $%04X does not return' % addr)
        return n


def build_ops():
    t = {}
    groups = {
        'lda': {'imm': 0xA9, 'zp': 0xA5, 'zpx': 0xB5, 'abs': 0xAD, 'abx': 0xBD, 'aby': 0xB9, 'izx': 0xA1, 'izy': 0xB1},
        'ldx': {'imm': 0xA2, 'zp': 0xA6, 'zpy': 0xB6, 'abs': 0xAE, 'aby': 0xBE},
        'ldy': {'imm': 0xA0, 'zp': 0xA4, 'zpx': 0xB4, 'abs': 0xAC, 'abx': 0xBC},
        'sta': {'zp': 0x85, 'zpx': 0x95, 'abs': 0x8D, 'abx': 0x9D, 'aby': 0x99, 'izx': 0x81, 'izy': 0x91},
        'stx': {'zp': 0x86, 'zpy': 0x96, 'abs': 0x8E},
        'sty': {'zp': 0x84, 'zpx': 0x94, 'abs': 0x8C},
        'adc': {'imm': 0x69, 'zp': 0x65, 'zpx': 0x75, 'abs': 0x6D, 'abx': 0x7D, 'aby': 0x79, 'izx': 0x61, 'izy': 0x71},
        'sbc': {'imm': 0xE9, 'zp': 0xE5, 'zpx': 0xF5, 'abs': 0xED, 'abx': 0xFD, 'aby': 0xF9, 'izx': 0xE1, 'izy': 0xF1},
        'and': {'imm': 0x29, 'zp': 0x25, 'zpx': 0x35, 'abs': 0x2D, 'abx': 0x3D, 'aby': 0x39, 'izx': 0x21, 'izy': 0x31},
        'ora': {'imm': 0x09, 'zp': 0x05, 'zpx': 0x15, 'abs': 0x0D, 'abx': 0x1D, 'aby': 0x19, 'izx': 0x01, 'izy': 0x11},
        'eor': {'imm': 0x49, 'zp': 0x45, 'zpx': 0x55, 'abs': 0x4D, 'abx': 0x5D, 'aby': 0x59, 'izx': 0x41, 'izy': 0x51},
        'cmp': {'imm': 0xC9, 'zp': 0xC5, 'zpx': 0xD5, 'abs': 0xCD, 'abx': 0xDD, 'aby': 0xD9, 'izx': 0xC1, 'izy': 0xD1},
        'cpx': {'imm': 0xE0, 'zp': 0xE4, 'abs': 0xEC},
        'cpy': {'imm': 0xC0, 'zp': 0xC4, 'abs': 0xCC},
        'bit': {'zp': 0x24, 'abs': 0x2C},
        'inc': {'zp': 0xE6, 'zpx': 0xF6, 'abs': 0xEE, 'abx': 0xFE},
        'dec': {'zp': 0xC6, 'zpx': 0xD6, 'abs': 0xCE, 'abx': 0xDE},
        'asl': {'acc': 0x0A, 'zp': 0x06, 'zpx': 0x16, 'abs': 0x0E, 'abx': 0x1E},
        'lsr': {'acc': 0x4A, 'zp': 0x46, 'zpx': 0x56, 'abs': 0x4E, 'abx': 0x5E},
        'rol': {'acc': 0x2A, 'zp': 0x26, 'zpx': 0x36, 'abs': 0x2E, 'abx': 0x3E},
        'ror': {'acc': 0x6A, 'zp': 0x66, 'zpx': 0x76, 'abs': 0x6E, 'abx': 0x7E},
        'jmp': {'abs': 0x4C, 'ind': 0x6C},
    }
    for name, modes in groups.items():
        for mode, op in modes.items():
            t[op] = (name, mode)
    single = {
        0x20: 'jsr', 0x60: 'rts', 0x40: 'rti', 0x00: 'brk', 0xEA: 'nop',
        0xAA: 'tax', 0xA8: 'tay', 0x8A: 'txa', 0x98: 'tya', 0xBA: 'tsx', 0x9A: 'txs',
        0x48: 'pha', 0x08: 'php', 0x68: 'pla', 0x28: 'plp',
        0xE8: 'inx', 0xC8: 'iny', 0xCA: 'dex', 0x88: 'dey',
        0x10: 'bpl', 0x30: 'bmi', 0x50: 'bvc', 0x70: 'bvs',
        0x90: 'bcc', 0xB0: 'bcs', 0xD0: 'bne', 0xF0: 'beq',
        0x18: 'clc', 0x38: 'sec', 0x58: 'cli', 0x78: 'sei',
        0xB8: 'clv', 0xD8: 'cld', 0xF8: 'sed',
    }
    for op, name in single.items():
        t[op] = (name, 'imp')
    return t


OPS = build_ops()


def main():
    path = sys.argv[1]
    song = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    calls = int(sys.argv[3]) if len(sys.argv) > 3 else 500
    d = open(path, 'rb').read()
    if d[:4] not in (b'PSID', b'RSID'):
        sys.exit('not a SID file')
    off, load, init, play = struct.unpack('>HHHH', d[6:14])
    data = d[off:]
    if load == 0:
        load = data[0] | (data[1] << 8)
        data = data[2:]
    if init == 0:
        init = load
    mem = bytearray(0x10000)
    mem[load:load + len(data)] = data
    mem[0], mem[1] = 0x2F, 0x37
    cpu = CPU(mem)
    cpu.call(init, song - 1)
    print('init: %d SID writes' % cpu.writes)
    for n in range(1, calls + 1):
        cpu.call(play)
        print('%d %s' % (n, ' '.join('%02X' % v for v in cpu.sid[:25])))


if __name__ == '__main__':
    main()
