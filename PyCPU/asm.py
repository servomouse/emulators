"""A small two-pass assembler for the CPU described in cpu.py.

Example::

    .org 0x0000
start:
    MV R0, #1
    CALL report
    JMP start

subroutine report at 0x0100
    OUT #0, R0
    RET

Labels and subroutine names are case-insensitive. ``;`` and ``//`` begin
comments. ``.org`` changes the output address; unfilled addresses are emitted
as zero bytes (NOP instructions). A bare label or ``[label]`` is an absolute
address; ``#value`` is an immediate value. For a jump, immediate values are
relative to the PC, while symbolic addresses are absolute.

Run ``python asm.py input.asm -o output.bin`` to assemble a source file.
"""

from __future__ import annotations

import argparse
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Union


# The numeric opcode values and operand codes mirror cpu.py.
OPCODES = {
    "NOP": 0x00,
    "MV": 0x01,
    "PUSH": 0x02,
    "POP": 0x03,
    "AND": 0x04,
    "OR": 0x05,
    "XOR": 0x06,
    "NOT": 0x07,
    "SFT": 0x08,
    "ADD": 0x09,
    "SUB": 0x0A,
    "CMP": 0x0B,
    "MUL": 0x0C,
    "DIV": 0x0D,
    "JMP": 0x0E,
    "JPZ": 0x0F,
    "JPG": 0x10,
    "JPL": 0x11,
    "CALL": 0x12,
    "RET": 0x13,
    "IN": 0x14,
    "OUT": 0x15,
    "INT": 0x16,
}

ARG_CODES = {
    name: code
    for code, name in enumerate(
        ("R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7",
         "F", "RA", "PC", "SP", "IMMED8", "IMMED16", "ADDR")
    )
}

REGISTERS = set(ARG_CODES) - {"IMMED8", "IMMED16", "ADDR"}
REG_OR_ADDR = REGISTERS | {"ADDR"}
VALUE = REG_OR_ADDR | {"IMMED8"}
JUMP_VALUE = VALUE | {"IMMED16"}

# For one-operand instructions the operand occupies the destination nibble.
# The unused source nibble is encoded as R0.
INSTRUCTION_OPERANDS = {
    "NOP": (),
    "MV": (VALUE, VALUE),
    "PUSH": (VALUE,),
    "POP": (REG_OR_ADDR,),
    "AND": (REG_OR_ADDR, VALUE),
    "OR": (REG_OR_ADDR, VALUE),
    "XOR": (REG_OR_ADDR, VALUE),
    "NOT": (REG_OR_ADDR,),
    "SFT": (VALUE, VALUE),
    "ADD": (REG_OR_ADDR, VALUE),
    "SUB": (REG_OR_ADDR, VALUE),
    "CMP": (REG_OR_ADDR, VALUE),
    "MUL": (REG_OR_ADDR, VALUE),
    "DIV": (REG_OR_ADDR, VALUE),
    "JMP": (JUMP_VALUE,),
    "JPZ": (JUMP_VALUE,),
    "JPG": (JUMP_VALUE,),
    "JPL": (JUMP_VALUE,),
    "CALL": ({"RA", "ADDR"},),
    "RET": (),
    "IN": (REG_OR_ADDR, REGISTERS | {"IMMED8"}),
    "OUT": (REGISTERS | {"IMMED8"}, REG_OR_ADDR),
    "INT": (REGISTERS | {"IMMED8"},),
}

MNEMONIC_ALIASES = {
    "MOV": "MV",
    "LD": "MV",
    "LOAD": "MV",
    "STORE": "MV",
}

LABEL_PATTERN = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
LABEL_AT_START = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*):\s*(.*)$")
REGISTER_PATTERN = re.compile(r"^(?:R[0-7]|F|RA|PC|SP)$", re.IGNORECASE)


class AssemblerError(ValueError):
    """Raised when assembly source cannot be translated to machine code."""


@dataclass(frozen=True)
class Operand:
    kind: str
    value: Union[int, str]

    @property
    def arg_code(self) -> int:
        return ARG_CODES[self.kind]

    @property
    def encoded_size(self) -> int:
        if self.kind in ("IMMED8",):
            return 1
        if self.kind in ("IMMED16", "ADDR"):
            return 2
        return 0


@dataclass(frozen=True)
class Instruction:
    line_number: int
    address: int
    mnemonic: str
    operands: tuple[Operand, ...]

    @property
    def size(self) -> int:
        if not self.operands:
            return 1
        return 2 + sum(operand.encoded_size for operand in self.operands)


def _error(line_number: int, message: str) -> AssemblerError:
    return AssemblerError(f"line {line_number}: {message}")


def _parse_number(text: str, line_number: int) -> int:
    text = text.strip().replace("_", "")
    try:
        if text.startswith("-$"):
            return -int(text[2:], 16)
        if text.startswith("$"):
            return int(text[1:], 16)
        if re.fullmatch(r"[+-]?\d+", text):
            return int(text, 10)
        return int(text, 0)
    except ValueError as exc:
        raise _error(line_number, f"invalid number {text!r}") from exc


def _parse_immediate(text: str, line_number: int) -> Operand:
    """Choose an immediate encoding from the value, unless width is explicit."""
    width_match = re.fullmatch(r"#(8|16)\((.+)\)", text.strip(), re.IGNORECASE)
    if width_match:
        width = int(width_match.group(1))
        value = _parse_number(width_match.group(2), line_number)
    else:
        value = _parse_number(text.strip()[1:], line_number)
        width = 8 if -128 <= value <= 0xFF else 16

    low, high = (-128, 0xFF) if width == 8 else (-0x8000, 0xFFFF)
    if not low <= value <= high:
        raise _error(line_number, f"{width}-bit immediate is out of range: {value}")
    return Operand(f"IMMED{width}", value)


def _parse_operand(text: str, line_number: int) -> Operand:
    text = text.strip()
    upper = text.upper()
    if REGISTER_PATTERN.fullmatch(text):
        return Operand(upper, 0)

    if text.startswith("#"):
        return _parse_immediate(text, line_number)

    if text.startswith("[") and text.endswith("]"):
        address = text[1:-1].strip()
        if not address:
            raise _error(line_number, "empty address operand")
        if LABEL_PATTERN.fullmatch(address):
            return Operand("ADDR", address.upper())
        value = _parse_number(address, line_number)
        if not 0 <= value <= 0xFFFF:
            raise _error(line_number, f"address is out of range: {value}")
        return Operand("ADDR", value)

    if LABEL_PATTERN.fullmatch(text):
        return Operand("ADDR", text.upper())

    # An unprefixed number is accepted as an immediate for convenient simple
    # programs; use brackets when the number is an absolute memory address.
    value = _parse_number(text, line_number)
    width = 8 if -128 <= value <= 0xFF else 16
    low, high = (-128, 0xFF) if width == 8 else (-0x8000, 0xFFFF)
    if not low <= value <= high:
        raise _error(line_number, f"immediate is out of range: {value}")
    return Operand(f"IMMED{width}", value)


def _remove_comment(line: str) -> str:
    # '#' is deliberately not a comment marker because it prefixes immediates.
    semicolon = line.find(";")
    slash = line.find("//")
    positions = [position for position in (semicolon, slash) if position >= 0]
    if positions:
        line = line[:min(positions)]
    return line.strip()


def _parse_origin(text: str, line_number: int) -> int:
    value = _parse_number(text, line_number)
    if not 0 <= value <= 0xFFFF:
        raise _error(line_number, f"origin is out of range: {value}")
    return value


def _parse_source(source: str) -> tuple[list[Instruction], dict[str, int]]:
    instructions: list[Instruction] = []
    labels: dict[str, int] = {}
    address = 0

    for line_number, original_line in enumerate(source.splitlines(), 1):
        line = _remove_comment(original_line)
        if not line:
            continue

        while True:
            match = LABEL_AT_START.match(line)
            if not match:
                break
            name, line = match.group(1).upper(), match.group(2).strip()
            if name in labels:
                raise _error(line_number, f"duplicate label {name!r}")
            labels[name] = address
            if not line:
                break
        if not line:
            continue

        origin_match = re.fullmatch(r"\.?ORG\s+(.+)", line, re.IGNORECASE)
        if origin_match:
            address = _parse_origin(origin_match.group(1), line_number)
            continue

        sub_match = re.fullmatch(
            r"(?:\.SUB(?:ROUTINE)?|SUBROUTINE)\s+([A-Za-z_][A-Za-z0-9_]*)"
            r"(?:\s+(?:AT\s+)?(.+))?",
            line,
            re.IGNORECASE,
        )
        if sub_match:
            name = sub_match.group(1).upper()
            origin = sub_match.group(2)
            if origin:
                address = _parse_origin(origin, line_number)
            if name in labels:
                raise _error(line_number, f"duplicate label {name!r}")
            labels[name] = address
            continue

        parts = line.split(None, 1)
        mnemonic = parts[0].upper()
        mnemonic = MNEMONIC_ALIASES.get(mnemonic, mnemonic)
        if mnemonic not in OPCODES:
            raise _error(line_number, f"unknown instruction {parts[0]!r}")

        operand_text = parts[1].strip() if len(parts) == 2 else ""
        operand_strings = [item.strip() for item in operand_text.split(",")] if operand_text else []
        if any(not item for item in operand_strings):
            raise _error(line_number, "empty operand")

        expected = INSTRUCTION_OPERANDS[mnemonic]
        if len(operand_strings) != len(expected):
            raise _error(
                line_number,
                f"{mnemonic} expects {len(expected)} operand(s), got {len(operand_strings)}",
            )

        operands = tuple(_parse_operand(item, line_number) for item in operand_strings)
        for index, (operand, allowed) in enumerate(zip(operands, expected), 1):
            if operand.kind not in allowed:
                allowed_text = ", ".join(sorted(allowed))
                raise _error(
                    line_number,
                    f"operand {index} of {mnemonic} must be one of: {allowed_text}",
                )
            if mnemonic == "SFT" and operand.kind == "IMMED8":
                if not -8 <= operand.value <= 8:
                    raise _error(line_number, "SFT immediate must be between -8 and 8")

        instruction = Instruction(line_number, address, mnemonic, operands)
        if address + instruction.size > 0x10000:
            raise _error(line_number, "instruction extends past address 0xFFFF")
        instructions.append(instruction)
        address += instruction.size

    return instructions, labels


def _resolve_operand(operand: Operand, labels: dict[str, int], line_number: int) -> int:
    if isinstance(operand.value, int):
        return operand.value
    try:
        return labels[operand.value]
    except KeyError as exc:
        raise _error(line_number, f"undefined label {operand.value!r}") from exc


def assemble(source: str) -> bytes:
    """Assemble source text into a flat binary image starting at address zero."""
    instructions, labels = _parse_source(source)
    image: dict[int, int] = {}

    for instruction in instructions:
        encoded = [OPCODES[instruction.mnemonic]]
        if instruction.operands:
            dst_code = instruction.operands[0].arg_code
            src_code = instruction.operands[1].arg_code if len(instruction.operands) > 1 else 0
            encoded.append((dst_code << 4) | src_code)

            for operand in instruction.operands:
                value = _resolve_operand(operand, labels, instruction.line_number)
                if operand.kind == "IMMED8":
                    encoded.append(value & 0xFF)
                elif operand.kind == "IMMED16":
                    encoded.extend(((value >> 8) & 0xFF, value & 0xFF))
                elif operand.kind == "ADDR":
                    if not 0 <= value <= 0xFFFF:
                        raise _error(instruction.line_number, f"address is out of range: {value}")
                    encoded.extend((value >> 8, value & 0xFF))

        for offset, byte in enumerate(encoded):
            location = instruction.address + offset
            if location in image:
                raise _error(
                    instruction.line_number,
                    f"instruction overlaps code at address 0x{location:04X}",
                )
            image[location] = byte

    if not image:
        return b""
    return bytes(image.get(address, 0) for address in range(max(image) + 1))


def assemble_file(source_path: Union[str, Path], output_path: Union[str, Path]) -> None:
    """Assemble a UTF-8 source file and write its flat binary image."""
    source = Path(source_path).read_text(encoding="utf-8")
    binary = assemble(source)
    Path(output_path).write_bytes(binary)


def main() -> int:
    parser = argparse.ArgumentParser(description="Assemble a custom CPU source file.")
    parser.add_argument("source", help="path to the assembly source")
    parser.add_argument("-o", "--output", required=True, help="path for the binary output")
    args = parser.parse_args()
    try:
        assemble_file(args.source, args.output)
    except AssemblerError as exc:
        parser.error(str(exc))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())