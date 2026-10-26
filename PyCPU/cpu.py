from alu import Flags, ALU

RESET_VECTOR = 0xFFFE

"""

=========================================================================

Interrupt Vector Table size is 32, interrupts can be individually enabled/disabled.
INT0 is Reset.
=========================================================================

Flags:
Z - zero
C - carry
N - negative

=========================================================================

Registers:
8-bit:
R0-R7 - General purpose registers
F - Flags
16-bit:
RA - Address register
PC
SP

=========================================================================

Opcodes:
(code) Mnemonics DST SRC

(0x00) NOP - No Operation

(0x01) MOV  R/(RA)/(ADDR), R/(RA)/(ADDR)/IMMED8 - Move one/two byte(s) from one location to another
(0x02) PUSH R/(RA)/(ADDR)/IMMED8
(0x03) POP  R/(RA)/(ADDR)

(0x04) AND R/(RA)/(ADDR), R/(RA)/(ADDR)/IMMED8 - Logical AND
(0x05) OR  R/(RA)/(ADDR), R/(RA)/(ADDR)/IMMED8 - Logical OR
(0x06) XOR R/(RA)/(ADDR), R/(RA)/(ADDR)/IMMED8 - Logical XOR
(0x07) NOT R/(RA)/(ADDR) - Bitwise inversion
(0x08) SFT R/(RA)/(ADDR)/IMMED8, R/(RA)/(ADDR)/IMMED8 - Cyclic shift to the left/right. SRC value is treated as a signed value, possible values are limited to [-8, 8]

(0x09) ADD R/(RA)/(ADDR), R/(RA)/(ADDR)/IMMED8
(0x0A) SUB R/(RA)/(ADDR), R/(RA)/(ADDR)/IMMED8
(0x0B) CMP R/(RA)/(ADDR), R/(RA)/(ADDR)/IMMED8
(0x0C) MUL R/(RA)/(ADDR), R/(RA)/(ADDR)/IMMED8
(0x0D) DIV R/(RA)/(ADDR), R/(RA)/(ADDR)/IMMED8

(0x0E) JMP R/(RA)/(ADDR)/IMMED8/IMMED16 - Unconditionaly jump       (add value to PC, R/IMMED8/IMMED16 - relative jump to a signed offset, (RA)/(ADDR) - absolute jump)
(0x0F) JPZ R/(RA)/(ADDR)/IMMED8/IMMED16 - Jump if zero flag is set  (add value to PC, R/IMMED8/IMMED16 - relative jump to a signed offset, (RA)/(ADDR) - absolute jump)
(0x10) JNZ R/(RA)/(ADDR)/IMMED8/IMMED16 - Jump if zero flag is set  (add value to PC, R/IMMED8/IMMED16 - relative jump to a signed offset, (RA)/(ADDR) - absolute jump)
(0x11) JPG R/(RA)/(ADDR)/IMMED8/IMMED16 - Jump if N flag is set     (add value to PC, R/IMMED8/IMMED16 - relative jump to a signed offset, (RA)/(ADDR) - absolute jump)
(0x12) JPL R/(RA)/(ADDR)/IMMED8/IMMED16 - Jump if N flag is cleared (add value to PC, R/IMMED8/IMMED16 - relative jump to a signed offset, (RA)/(ADDR) - absolute jump)

(0x13) CALL (RA)/(ADDR) - Push R0-R7, RA, SP and PC and load the new value to PC
(0x14) RET - Pop R0-R7, RA, SP and PC

(0x15) IN  R/(RA)/(ADDR), R/IMMED8 - Read one byte from io_space
(0x16) OUT R/IMMED8, R/(RA)/(ADDR) - Write one byte to io_space

(0x17) INT R/IMMED8 - Trigger an interrupt

Serice opcodes:
(0x18) IVT IMMED16 - Set Interrupt Vector Table offset (default 0x0000)
(0x19) EI R/IMMED8 - Enable Interrupt number specified as an argument
(0x1A) DI R/IMMED8 - Disable Interrupt number specified as an argument

=========================================================================

SRC/DST encoding:
[R0, R1, R2, R3, R4, R5, R6, R7, F, RA, PC, SP, IMMED8, IMMED16, ADDR, (RA)]

=========================================================================

Command encoding:
Byte 0:
    bits 0-5: opcode
    bits 5-8: reserved

Byte 1:
    bits 0-5: DST encoding
    bits 5-8: SRC encoding

Bytes 2-6: Optional IMMED8/IMMED16/ADDR values

"""

class CPU:
    def __init__(self, mem_space, io_space):
        self.arg_names = ["R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7", "F", "RA", "PC", "SP", "IMMED8", "IMMED16", "ADDR"]
        self.arg_types = {
            "R0": "REG",
            "R1": "REG",
            "R2": "REG",
            "R3": "REG",
            "R4": "REG",
            "R5": "REG",
            "R6": "REG",
            "R7": "REG",
            "F":  "REG",
            "RA": "REG",
            "PC": "REG",
            "SP": "REG",
            "IMMED8": "IMMED",
            "IMMED16": "IMMED",
            "ADDR": "ADDR"
        }
        self.reg_names = ["R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7", "F", "RA", "PC", "SP"]
        self._8_bit_regs = ["R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7"]
        self.mem_space = mem_space
        self.io_space = io_space
        self.flags = Flags()
        self.alu = ALU(self.flags)
        self.reset()
        self.opcode_executors = [self.NOP for _ in range(0xFF)]

    def reset(self):
        self.registers = {
            # 8-bit registers:
            "R0": 0,
            "R1": 0,
            "R2": 0,
            "R3": 0,
            "R4": 0,
            "R5": 0,
            "R6": 0,
            "R7": 0,
            "F":  0,
            # 16-bit registers:
            "RA": 0,
            "SP": 0,
            "PC": RESET_VECTOR,
        }

    def tick(self):
        opcode = self.mem_space.read(self.registers["PC"])
        self.opcode_executors[opcode](opcode)
        if self.registers["PC"] > 0xFFFF:
            self.registers["PC"] = 0

    def get_arg_value(self, arg_code):
        if arg_code >= len(self.arg_names):
            raise Exception(f"Error: Invalid arg_code {arg_code} at {self.registers['PC']}")
        arg_name = self.arg_names[arg_code]
        if arg_name in self.reg_names:
            return self.registers[arg_name]
        self.registers["PC"] += 1
        if arg_name == "IMMED8":
            return self.mem_space.read(self.registers["PC"])
        MSB = self.mem_space.read(self.registers["PC"])
        self.registers["PC"] += 1
        LSB = self.mem_space.read(self.registers["PC"])
        val = (MSB << 8) | LSB
        return val

    def set_arg_value(self, arg_name, addr, value):
        if arg_name in self.reg_names:
            if arg_name in self._8_bit_regs:
                self.registers[arg_name] = value & 0xFF
            else:
                self.registers[arg_name] = value
        elif arg_name == "ADDR":
            self.mem_space.write(addr, value)


    def arg_decoder(self, value):
        dst_code = (value >> 4) & 0x0F
        src_code  = value & 0x0F
        return dst_code, self.get_arg_value(dst_code), self.get_arg_value(src_code)

    def NOP(self, opcode):
        self.registers["PC"] += 1

    def move(self, opcode):
        self.registers["PC"] += 1
        args = self.mem_space.read(self.registers["PC"])
        dst_name, dst_val, src_val = self.arg_decoder(args)
