from alu import Flags, ALU

RESET_VECTOR = 0xFFFE

"""
=========================================================================

Flags:
Z - zero
C - carry
N - negative

=========================================================================

Registers:
8-bit:
R0-R7 - General purpose registers
FLAGS
16-bit:
RA - Address register
PC
SP

=========================================================================

Opcodes:

(0x00) NOP - No Operation

(0x01) MV   R/(RA)/(ADDR), R/(RA)/(ADDR)/IMMED8/IMMED16 - Move one/two byte(s) from one location to another
(0x02) PUSH R/(RA)/(ADDR)/IMMED8/IMMED16
(0x03) POP  R/(RA)/(ADDR)

(0x04) AND R/(RA)/(ADDR), R/(RA)/(ADDR)/IMMED8/IMMED16 - Logical AND
(0x05) OR  R/(RA)/(ADDR), R/(RA)/(ADDR)/IMMED8/IMMED16 - Logical OR
(0x06) XOR R/(RA)/(ADDR), R/(RA)/(ADDR)/IMMED8/IMMED16 - Logical XOR
(0x07) NOT R/(RA)/(ADDR) - Bitwise inversion

(0x08) ADD R/(RA)/(ADDR), R/(RA)/(ADDR)/IMMED8/IMMED16
(0x09) SUB R/(RA)/(ADDR), R/(RA)/(ADDR)/IMMED8/IMMED16
(0x0A) CMP R/(RA)/(ADDR), R/(RA)/(ADDR)/IMMED8/IMMED16
(0x0B) MUL R/(RA)/(ADDR), R/(RA)/(ADDR)/IMMED8/IMMED16
(0x0C) DIV R/(RA)/(ADDR), R/(RA)/(ADDR)/IMMED8/IMMED16

(0x0D) JMP R/(RA)/(ADDR)/IMMED8/IMMED16 - Unconditionaly jump       (add value to PC, R/IMMED8/IMMED16 - relative jump, (RA)/(ADDR) - absolute jump)
(0x0E) JPZ R/(RA)/(ADDR)/IMMED8/IMMED16 - Jump if zero flag is set  (add value to PC, R/IMMED8/IMMED16 - relative jump, (RA)/(ADDR) - absolute jump)
(0x0F) JPG R/(RA)/(ADDR)/IMMED8/IMMED16 - Jump if N flag is set     (add value to PC, R/IMMED8/IMMED16 - relative jump, (RA)/(ADDR) - absolute jump)
(0x10) JPL R/(RA)/(ADDR)/IMMED8/IMMED16 - Jump if N flag is cleared (add value to PC, R/IMMED8/IMMED16 - relative jump, (RA)/(ADDR) - absolute jump)

(0x11) CALL (RA)/(ADDR) - Push R0-R7, SP and PC and load the value into PC
(0x12) RET - Pop R0-R7, SP and PC

(0x13) IN  R/(RA)/(ADDR), R/IMMED8 - Read one byte from io_space
(0x14) OUT R/IMMED8, R/(RA)/(ADDR) - Write one byte to io_space

(0x15) INT R/IMMED8 - Trigger an interrupt

=========================================================================

SRC/DST encoding:
[R0, R1, R2, R3, R4, R5, R6, R7, F, RA, PC, SP, IMMED8, IMMED16, ADDR]

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

    def NOP(self, opcode):
        self.registers["PC"] += 1
