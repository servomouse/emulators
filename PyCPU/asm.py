

special_symbols = ["SECTION", "section",
                   "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7", "RA",
                   "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "ra",
                   "EQU", "DB", "DW", "equ", "db", "dw"
]

# MOV Rx, RA ; Results depends on the destination: Odd registers get/give LSB, even registers - MSB. Examples:
# MOV RA, 0x1234
# MOV R0, RA ; R0 = 0x34
# MOV R1, RA ; R1 = 0x12
# MOV RA, R0 ; Moves the LSB
# MOV RA, R1 ; Moves the MSB

opcodes = {
    "NOP":  {"bin": "00", "dst": None, "src": None},
    "MOV":  {"bin": "01", "dst": ["R", "RA", "[RA]", "[IMMED16]"], "src": ["R", "RA", "[RA]", "IMMED8", "IMMED16", "[IMMED16]"]},
    "SWAP": {"bin": "02", "dst": ["R", "RA", "[RA]", "[IMMED16]"], "src": ["R", "RA", "[RA]", "IMMED8", "IMMED16", "[IMMED16]"]},
    "PUSH": {"bin": "03", "dst": None, "src": ["R", "RA", "[RA]", "IMMED8", "[IMMED16]"]},
    "POP":  {"bin": "04", "dst": ["R", "RA", "[RA]", "[IMMED16]"], "src": None},
    "AND":  {"bin": "05", "dst": ["R", "RA", "[RA]", "[IMMED16]"], "src": ["R", "RA", "[RA]", "IMMED8", "[IMMED16]"]},
    "OR":   {"bin": "06", "dst": ["R", "RA", "[RA]", "[IMMED16]"], "src": ["R", "RA", "[RA]", "IMMED8", "[IMMED16]"]},
    "XOR":  {"bin": "07", "dst": ["R", "RA", "[RA]", "[IMMED16]"], "src": ["R", "RA", "[RA]", "IMMED8", "[IMMED16]"]},
    "NOT":  {"bin": "08", "dst": ["R", "RA", "[RA]", "[IMMED16]"], "src": None},
    "SFT":  {"bin": "09", "dst": ["R", "RA", "[RA]", "[IMMED16]"], "src": ["R", "RA", "[RA]", "IMMED8", "[IMMED16]"]},
    "ADD":  {"bin": "0a", "dst": ["R", "RA", "[RA]", "[IMMED16]"], "src": ["R", "RA", "[RA]", "IMMED8", "[IMMED16]"]},
    "SUB":  {"bin": "0b", "dst": ["R", "RA", "[RA]", "[IMMED16]"], "src": ["R", "RA", "[RA]", "IMMED8", "[IMMED16]"]},
    "CMP":  {"bin": "0c", "dst": ["R", "RA", "[RA]", "[IMMED16]"], "src": ["R", "RA", "[RA]", "IMMED8", "[IMMED16]"]},
    "MUL":  {"bin": "0d", "dst": ["R", "RA", "[RA]", "[IMMED16]"], "src": ["R", "RA", "[RA]", "IMMED8", "[IMMED16]"]},
    "DIV":  {"bin": "0e", "dst": ["R", "RA", "[RA]", "[IMMED16]"], "src": ["R", "RA", "[RA]", "IMMED8", "[IMMED16]"]},
    "JMP":  {"bin": "0f", "dst": ["R", "[RA]", "IMMED8", "IMMED16", "[IMMED16]"], "src": None},
    "JPZ":  {"bin": "10", "dst": ["R", "[RA]", "IMMED8", "IMMED16", "[IMMED16]"], "src": None},
    "JNZ":  {"bin": "11", "dst": ["R", "[RA]", "IMMED8", "IMMED16", "[IMMED16]"], "src": None},
    "JPG":  {"bin": "12", "dst": ["R", "[RA]", "IMMED8", "IMMED16", "[IMMED16]"], "src": None},
    "JPL":  {"bin": "13", "dst": ["R", "[RA]", "IMMED8", "IMMED16", "[IMMED16]"], "src": None},
    "CALL": {"bin": "14", "dst": ["[RA]", "[IMMED16]"], "src": None},
    "RET":  {"bin": "15", "dst": None, "src": None},
    "IN":   {"bin": "16", "dst": ["R", "RA", "[RA]", "[IMMED16]"], "src": ["R", "IMMED8"]}, # SRC is a port
    "OUT":  {"bin": "17", "dst": ["R", "IMMED8"], "src": ["R", "RA", "[RA]", "[IMMED16]"]}, # DST is a port
    "INT":  {"bin": "18", "dst": ["R", "IMMED8"], "src": None},
    "IVT":  {"bin": "19", "dst": ["IMMED16"], "src": None},
    "EI":   {"bin": "1a", "dst": ["R", "IMMED8"], "src": None},
    "DI":   {"bin": "1b", "dst": ["R", "IMMED8"], "src": None},
}


def to_hex_with_zeros(val: int, bits: int = 32) -> str:
    """Converts an integer to a hex string with leading zeros using two's complement for negatives."""
    # Mask the value to fit within the specified bit-width (handles two's complement)
    mask = (1 << bits) - 1
    masked_val = val & mask
    
    # Calculate how many hex characters are needed (each hex char is 4 bits)
    hex_len = bits // 4
    
    # Format with leading zeros, lowercase hex, and no '0x' prefix
    return f"{masked_val:0{hex_len}x}"


def is_integer(s):
    try:
        int(s.strip(), base=0)
        return True
    except ValueError:
        return False


def get_integer_value(s):
    return int(s.strip(), base=0)


def int_size(val):
    """
    Checks what standard integer type a value fits into.
    Order of preference: INT8 -> UINT8 -> INT16 -> UINT16.
    """
    # 8-bit checks
    if -128 <= val <= 127:
        return "INT8"
    if 0 <= val <= 255:
        return "UINT8"
        
    # 16-bit checks
    if -32768 <= val <= 32767:
        return "INT16"
    if 0 <= val <= 65535:
        return "UINT16"
        
    return "ERROR"


def extract_integer(ln, text):
    int_value = ""
    if text.startswith("0x"):
        int_value += "0x"
        for i in range(2, len(text)):
            if text[i] in "0123456789ABCDEFabcdef":
                int_value += text[i]
            elif text[i] in [" ", ","]:
                break
            else:
                raise Exception(f"Syntax error: Unexpected symbol {text[i]} on line {ln}!")
        return int_value
    elif text.startswith("0b"):
        int_value += "0b"
        for i in range(2, len(text)):
            if text[i] in "01":
                int_value += text[i]
            elif text[i] in [" ", ","]:
                break
            else:
                raise Exception(f"Syntax error: Unexpected symbol {text[i]} on line {ln}!")
        return int_value
    for i in range(len(text)):
        if text[i] in "0123456789":
            int_value += text[i]
        elif text[i] in [" ", ","]:
            break
        else:
            raise Exception(f"Syntax error: Unexpected symbol {text[i]} on line {ln}!")
    return int_value


def extract_string(ln, text):
    if text[0] in ["'", "\""]:
        quote = text[0]
        ret_string = text[0]
        for i in range(1, len(text)):
            ret_string += text[i]
            if text[i] == quote:
                break
        if text[i] != quote:
            raise Exception(f"Syntax error: Unclosed parenthesis at line {ln}: {text}")
        return ret_string
    return None


def get_data_item(ln, text):
    text = text.strip()
    if text[0] in ["'", "\""]:
        return "text", extract_string(ln, text)
    elif text[0] in "0123456789":
        return "number", extract_integer(ln, text)
    text_split = text.split(",", maxsplit=1)
    if is_label(ln, text_split[0]):
        return "label", text_split[0]
    elif is_expression(ln, text_split[0]):
        return "expression", text_split[0]
    return None, ""


def is_label(line_number, text):
    text = text.strip()
    allowed_symbols = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_"
    digits = "0123456789"
    if text[0] not in allowed_symbols:
        return False
    for i in range(1, len(text)):
        if text[i] not in allowed_symbols+digits:
            return False
    if text in special_symbols:
        raise Exception(f"Syntax error: Invalid label at {line_number}: {text} (do not use special symbols as labels)")
    return True


def is_expression(line_number, expression):
    MAX_PASS_NUM = 100
    math_operations = ["+", "-", "*", "/"]
    simbols = ["(", ")"]

    def _parse_array(arr):
        # print(f"_parse_array: {arr = }")
        sub_steps = []
        for _ in range(len(arr)):
            if len(arr) == 0:
                break
            element = arr.pop(0)
            if is_integer(element) or is_label(line_number, element):
                sub_steps.append(element)
            elif element in math_operations:
                sub_steps.append(element)
            elif element == "(":
                sub_step, leftover = _parse_array(arr)
                sub_steps.append(sub_step)
                arr = leftover
            elif element == ")":
                break
        return sub_steps, arr

    def _check_expression(expr):
        last_op = True
        for i in range(len(expr)):
            if last_op:
                if isinstance(expr[i], list):
                    if not _check_expression(expr[i]):
                        return False
                elif not (is_integer(expr[i]) or is_label(line_number, expr[i])):
                    return False
                last_op = False
            else:
                if expr[i] not in math_operations:
                    return False
                last_op = True
        return True

    for op in math_operations+simbols:
        expression = expression.replace(op, f" {op} ")
    expression = expression.split()
    expression, _ = _parse_array(expression)
    return _check_expression(expression)


def parse_expression(line_number, expression):

    MAX_PASS_NUM = 10
    math_operations = ["+", "-", "*", "/"]
    simbols = ["(", ")"]

    def _parse_array(arr):
        # print(f"_parse_array: {arr = }")
        sub_steps = []
        for _ in range(len(arr)):
            if len(arr) == 0:
                break
            element = arr.pop(0)
            if is_integer(element) or is_label(line_number, element):
                sub_steps.append(element)
            elif element in math_operations:
                sub_steps.append(element)
            elif element == "(":
                sub_step, leftover = _parse_array(arr)
                sub_steps.append(sub_step)
                arr = leftover
            elif element == ")":
                break
        return sub_steps, arr

    def _prepare_expression(expr):
        # print(f"DEBUG::: {expr = }")
        if len(expr) == 3:
            if isinstance(expr[0], list):
                expr[0] = _prepare_expression(expr[0])
            if isinstance(expr[2], list):
                expr[2] = _prepare_expression(expr[2])
            return expr
        for _ in range(MAX_PASS_NUM):
            index = -1
            if isinstance(expr[0], list):
                # print(f"DEBUG::: expr[0] is a list: {expr[0] = }, {expr[0][0] = }")
                expr[0] = _prepare_expression(expr[0])
                print(f"DEBUG::: After recursive call {expr = }")
            for i in range(1, len(expr), 2):
                if isinstance(expr[i+1], list):
                    # print(f"DEBUG::: expr[i+1] is a list: {i = }, {expr[i+1] = }")
                    expr[i] = _prepare_expression(expr[i])
                if expr[i] in ["*", "/"]:
                    index = i-1
                    break
            if index > -1:
                v0 = expr[index]
                v1 = expr[index+1]
                v2 = expr[index+2]
                expr.pop(index+2)
                expr.pop(index+1)
                expr[index] = [v0, v1, v2]
        return expr

    def _parse_expression(expr):
        new_expr = []
        # print(f"DEBUG::: _parse_expression {expr = }")
        element0 = expr.pop(0)
        if isinstance(element0, list):
            new_expr.append(_parse_expression(element0))
        else:
            new_expr.append(element0)
        for _ in range(len(expr)):
            element1 = expr.pop(0)
            element2 = expr.pop(0)
            if element1 not in math_operations:
                raise Exception(f"Error: invalid expression {expression} at {line_number}")
            if isinstance(element2, list):
                new_expr.append(_parse_expression(element2))
            else:
                new_expr.append(element2)
            new_expr.append(element1)
            if len(expr) == 0:
                break
        # print(f"DEBUG::: _parse_expression {new_expr = }")
        return new_expr

    for op in math_operations+simbols:
        expression = expression.replace(op, f" {op} ")
    expression = expression.split()
    _labels = []
    for item in expression:
        if is_label(line_number, item) and item not in _labels:
            _labels.append(item)
    expression, _ = _parse_array(expression)
    # print(f"DEBUG::: {expression = }")
    expression = _prepare_expression(expression)
    parsed_expr = _parse_expression(expression)
    return parsed_expr, _labels


class OpcodeProcessor:
    def __init__(self, opcode_table):
        self.opcode_table = opcode_table
        self.registers = ["R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7", "F", "RA", "PC", "SP"]

        # SRC/DST encoding:
        self.arg_encoding = self.registers + ["[RA]", "IMMED8", "IMMED16", "[IMMED16]"]

    def is_opcode(self, text):
        return text in self.opcode_table

    def _get_arg_code(self, arg):
        return to_hex_with_zeros(self.arg_encoding.index(arg), 4)

    def _encode_opcode(self, opcode, dst, src, ln, original_text):
        op_code = self.opcode_table[opcode]["bin"]
        if dst is None and src is None:
            return {"type": "bin", "data": op_code, "size": 1, "ln": ln, "original_text": original_text}
        if dst is None and src is not None: # Opcodes with SRC only: [PUSH]
            if src["type"] == "bin":
                arg_code = "0" + src["arg_code"]
                full_binary = op_code + arg_code + src["arg_bin"]
                return {"type": "bin", "data": full_binary, "size": len(full_binary)//2, "ln": ln, "original_text": original_text}
            else:
                opcode_size = 2 + (len(src['arg_bin'])//2)
                return {"type": "opc", "data": op_code, "size": opcode_size, "args": {"dst": None, "src": src}, "ln": ln, "original_text": original_text}
        elif dst is not None and src is None:
            if dst["type"] == "bin":
                # op_code = self.opcode_table[opcode]["bin"]
                arg_code = dst["arg_code"] + "0"
                full_binary = op_code + arg_code + dst["arg_bin"]
                return {"type": "bin", "data": full_binary, "size": len(full_binary)//2, "ln": ln, "original_text": original_text}
            else:
                opcode_size = 2 + (len(dst['arg_bin'])//2)
                return {"type": "opc", "data": op_code, "size": opcode_size, "args": {"dst": dst, "src": None}, "ln": ln, "original_text": original_text}
        else:
            if dst["type"] == "bin" and src["type"] == "bin":
                # op_code = self.opcode_table[opcode]["bin"]
                arg_code = dst["arg_code"] + src["arg_code"]
                full_binary = op_code + arg_code + dst["arg_bin"] + src["arg_bin"]
                return {"type": "bin", "data": full_binary, "size": len(full_binary)//2, "ln": ln, "original_text": original_text}
            else:
                opcode_size = 2 + (len(dst['arg_bin'])//2) + (len(src['arg_bin'])//2)
                return {"type": "opc", "data": op_code, "size": opcode_size, "args": {"dst": dst, "src": src}, "ln": ln, "original_text": original_text}

    def decode_arg(self, ln, argument, constants):
        arg = argument.strip()
        if arg in self.registers:
            return {"arg_code": self._get_arg_code(arg), "arg_bin": "", "type": "bin"}
        elif arg == "[RA]":
            return {"arg_code": self._get_arg_code("[RA]"), "arg_bin": "", "type": "bin"}
        elif arg == "$":
            return {"arg_code": self._get_arg_code("IMMED16"), "arg_bin": "----", "type": "label", "dependencies": [arg]}
        elif is_label(ln, arg):
            if arg in constants:
                val = constants[arg]["value"]
                val_size = int_size(val)
                if val_size in ["INT8", "UINT8"]:
                    return {"arg_code": self._get_arg_code("IMMED8"), "arg_bin": to_hex_with_zeros(val, 8), "type": "bin"}
                else:
                    return {"arg_code": self._get_arg_code("IMMED16"), "arg_bin": to_hex_with_zeros(val, 16), "type": "bin"}
            else:
                return {"arg_code": self._get_arg_code("IMMED16"), "arg_bin": "----", "type": "label", "dependencies": [arg]}
        elif is_integer(arg):
            arg_value = get_integer_value(arg)
            arg_size = int_size(arg_value)
            if arg_size in ["INT8", "UINT8"]:
                return {"arg_code": self._get_arg_code("IMMED8"), "arg_bin": to_hex_with_zeros(arg_value, 8), "type": "bin"}
            elif arg_size in ["INT16", "UINT16"]:
                return {"arg_code": self._get_arg_code("IMMED16"), "arg_bin": to_hex_with_zeros(arg_value, 16), "type": "bin"}
            else:
                raise Exception(f"Syntax error: Invalid argument at line {ln}: {arg} (Value doesn't fit into 16 bit)")
        elif arg[0] == "[" and arg[-1] == "]" and is_label(ln, arg[1:-1]):
            arg_label = arg.replace("[", "").replace("]", "")
            return {"arg_code": self._get_arg_code("[IMMED16]"), "arg_bin": "----", "type": "label", "dependencies": [arg_label]}
        elif is_expression(ln, arg):
            expr, deps = parse_expression(ln, arg)
            deps_resolved = True
            deps_dict = {}
            for d in deps:
                if d in constants:
                    deps_dict[d] = constants[d]["value"]
                else:
                    deps_resolved = False
                    break
            if deps_resolved:
                val = resolve_expression(ln, expr, deps_dict)
                val_size = int_size(val)
                if val_size in ["INT8", "UINT8"]:
                    return {"arg_code": self._get_arg_code("IMMED8"), "arg_bin": to_hex_with_zeros(val, 8), "type": "bin"}
                else:
                    return {"arg_code": self._get_arg_code("IMMED16"), "arg_bin": to_hex_with_zeros(val, 16), "type": "bin"}
            return {"arg_code": self._get_arg_code("IMMED16"), "arg_bin": "----", "type": "expression", "expression": expr, "dependencies": deps}
        else:
            raise Exception(f"Syntax error: Invalid argument at line {ln}: {arg}")

    def parse_opcode(self, line, constants):
        ln = line["line"]
        text = line["text"]
        substrs = text.split(maxsplit=1)
        opcode, args = (substrs[0], substrs[1]) if len(substrs) > 1 else (substrs[0], "")
        if opcode in ["NOP", "RET"]:    # Opcodes without arguments
            return self._encode_opcode(opcode, None, None, ln, text)
        elif opcode == "PUSH": # Opcodes with SRC only
            arg_dict = self.decode_arg(ln, args, constants)
            return self._encode_opcode(opcode, None, arg_dict, ln, text)
        elif opcode in ["POP", "NOT", "JMP", "JPZ", "JNZ", "JPG", "JPL", "CALL", "INT", "IVT", "EI", "DI"]: # Opcodes with DST only
            arg_dict = self.decode_arg(ln, args, constants)
            return self._encode_opcode(opcode, arg_dict, None, ln, text)
        elif opcode in ["MOV", "SWAP", "AND", "OR", "XOR", "SFT", "ADD", "SUB", "CMP", "MUL", "DIV", "IN", "OUT"]:
            # Opcodes with two arguments
            dst, src = args.split(",", maxsplit=1)
            dst_arg_dict = self.decode_arg(ln, dst, constants)
            src_arg_dict = self.decode_arg(ln, src, constants)
            return self._encode_opcode(opcode, dst_arg_dict, src_arg_dict, ln, text)
        else:
            raise Exception(f"Assembler internal error: Opcode {opcode} was not processed")

    def get_dependencies(self, opcode):
        # {'type': 'opc', 'data': '01', 'args': {
        #     'dst': {'arg_code': 'f', 'arg_bin': '----', 'type': 'label', 'dependencies': ['VAR_COUNTER']}, 
        #     'src': {'arg_code': '0', 'arg_bin': '', 'type': 'bin'}
        # }, 'ln': 47, 'original_text': 'MOV [VAR_COUNTER], R0'}
        dst = opcode["args"]["dst"]
        src = opcode["args"]["src"]
        deps = []
        if dst is not None and dst["type"] in ["label", "expression"]:
            deps += dst["dependencies"]
        if src is not None and src["type"] in ["label", "expression"]:
            deps += src["dependencies"]
        return deps

    def resolve_opcode(self, opcode, deps_dict):
        dst = opcode["args"]["dst"]
        src = opcode["args"]["src"]
        op_code = opcode["data"]
        arg_code = ""
        dst_bin = ""
        src_bin = ""
        if dst is None:
            arg_code += "0"
        else:
            arg_code += dst["arg_code"]
            if dst["type"] == "bin":
                dst_bin += dst["arg_bin"]
            elif dst["type"] == "label":
                deps = dst["dependencies"]
                dst_bin += to_hex_with_zeros(deps_dict[deps[0]], 16)
            elif dst["type"] == "expression":
                expr = dst["expression"]
                dst_bin += to_hex_with_zeros(resolve_expression(opcode["ln"], expr, deps_dict), 16)
        if src is None:
            arg_code += "0"
        else:
            arg_code += src["arg_code"]
            if src["type"] == "bin":
                src_bin += src["arg_bin"]
            elif src["type"] == "label":
                deps = src["dependencies"]
                src_bin += to_hex_with_zeros(deps_dict[deps[0]], 16)
            elif src["type"] == "expression":
                expr = src["expression"]
                src_bin += to_hex_with_zeros(resolve_expression(opcode["ln"], expr, deps_dict), 16)
        return op_code + arg_code + dst_bin + src_bin


def parse_data(data, constants):
    ln = data["line"]
    text = data["text"]
    data_type, values = text.split(maxsplit=1)
    if data_type in ["UINT8", "uint8", "CHAR"]:
        num_bytes = 1
        num_bits = 8
    elif data_type in ["UINT16", "uint16"]:
        num_bytes = 2
        num_bits = 16
    else:
        raise Exception(f"Error: Unsupported data type {data_type} on line {ln}")
    MAX_ITEMS_NUM = 32
    items = []
    dependencies = []
    data_size = 0
    for _ in range(MAX_ITEMS_NUM):
        item_type, item = get_data_item(ln, values)
        if item_type == None:
            break
        if item_type == "number":
            items.append(["bin", to_hex_with_zeros(get_integer_value(item), num_bits)])
            data_size += num_bytes
        elif item_type == "text":
            bare_string = item[1:-1]
            string_bin = ""
            for c in bare_string:
                string_bin += to_hex_with_zeros(ord(c), 8)
            items.append(["bin", string_bin])
            data_size += len(bare_string)
        elif item_type == "label":
            if item in constants:
                items.append(["bin", to_hex_with_zeros(constants[item]["value"], num_bits)])
            else:
                if num_bytes == 2:
                    items.append(["label", "----", item])
                    dependencies.append(item)
                else:
                    raise Exception(f"Syntax error: Cannot use label as an 8-bit value (line {ln}, {text = })")
            data_size += num_bytes
        elif item_type == "expression":
            expr, deps = parse_expression(ln, item)
            deps_resolved = True
            deps_dict = {}
            for d in deps:
                if d in constants:
                    deps_dict[d] = constants[d]["value"]
                else:
                    deps_resolved = False
                    break
            if deps_resolved:
                val = resolve_expression(ln, expr, deps_dict)
                items.append(["bin", to_hex_with_zeros(val, num_bits)])
            else:
                items.append(["expression", "----", item])
                dependencies += deps
            data_size += num_bytes
        values = values[len(item):].strip()
        if values.startswith(","):
            values = values[1:]
        if len(values) == 0:
            break
    return {"type": "data", "offset": -1, "data": "", "size": data_size, "items": items, "ln": ln, "original_text": text}


def parse_constant(line_number, name, value):
    if is_integer(value):
        return name, {"type": "int", "value": get_integer_value(value)}
    elif is_label(line_number, value):
        return name, {"type": "label", "value": value}
    elif is_expression(line_number, value):
        expr, dependencies = parse_expression(line_number, value)
        return name, {"type": "expression", "expression": expr, "dependencies": dependencies}
    else:
        raise Exception(f"Parsing error: Invalid expression on line {line_number}: {value}")


def parse_section(ln, text_split, orig_text, sections):
    print(f"DEBUG::: Parsing section {text_split}")
    current_section = ""
    if len(text_split) == 2:
        section_name = text_split[1]
        if not is_label(ln, section_name):
            raise Exception(f"Syntax error: Invalid section name at {ln}: {section_name}")
        if section_name in sections:
            current_section = section_name
        else:
            raise Exception(f"Error: Line {ln}: Using section {section_name} before specifying its offset! Set the offset like SECTION section_name, 0xABCD")
    elif len(text_split) == 3:
        section_name = text_split[1].replace(",", " ")
        if not is_label(ln, section_name):
            raise Exception(f"Syntax error: Invalid section name at line {ln}: {section_name}")
        current_section = section_name
        section_offset = text_split[2]
        if is_integer(section_offset):
            section_offset = get_integer_value(section_offset)
        elif is_label(ln, section_offset):
            section_offset = section_offset
        else:
            raise Exception(f"Syntax error: Invalid section offset definition at {ln}: {orig_text}")
        if section_name in sections:
            raise Exception(f"Syntax error: Invalid section offset redefinition at {ln}: {orig_text}")
        sections[section_name] = {"offset": section_offset, "blocks": {}}
    return current_section


def resolve_expression(line_number, expr, deps):

    def _prepare_subexpr(subexpr):
        for i in range(len(subexpr)):
            if isinstance(subexpr[i], list):
                _prepare_subexpr(subexpr[i])
            elif is_label(line_number, subexpr[i]):
                subexpr[i] = deps[subexpr[i]]
            elif is_integer(subexpr[i]):
                subexpr[i] = get_integer_value(subexpr[i])
        return subexpr

    def _solve_expr(subexpr):
        if isinstance(subexpr[0], list):
            res = _solve_expr(subexpr[0])
        elif isinstance(subexpr[0], int):
            res = subexpr[0]
        else:
            raise Exception(f"Internal error: Should never get here, but we have: {expr = }, {deps = }, invalid argument: {subexpr[0]}")
        for i in range(1, len(subexpr), 2):
            if isinstance(subexpr[i], list):
                val1 = _solve_expr(subexpr[i])
            elif isinstance(subexpr[i], int):
                val1 = subexpr[i]
            else:
                raise Exception(f"Internal error: Should never get here, but we have: {expr = }, {deps = }, invalid argument: {subexpr[i]}")
            op = subexpr[i+1]
            if op == "+":
                res = res + val1
            elif op == "-":
                res = res - val1
            elif op == "*":
                res = res * val1
            elif op == "/":
                res = res // val1
            else:
                raise Exception(f"Internal error: Should never get here, but we have: {expr = }, {deps = }, invalid operation: {op}")
            if i >= len(subexpr):
                break
        return res

    expr = _prepare_subexpr(expr)
    return _solve_expr(expr)


def resolve_constants(consts):
    MAX_PASSES_NUM = 2
    for pass_num in range(MAX_PASSES_NUM):
        done = True
        for c in consts:
            if consts[c]["type"] == "int":
                continue
            if consts[c]["type"] == "label":
                c_name = consts[c]["value"]
                if consts[c_name]["type"] == "int":
                    consts[c]["value"] = consts[c_name]["value"]
                    consts[c]["type"] = "int"
                else:
                    done = False
            elif consts[c]["type"] == "expression":
                dependensies = consts[c]["dependencies"]
                dep_values = {}
                for d in dependensies:
                    if consts[d]["type"] == "int":
                        dep_values[d] = consts[d]["value"]
                    else:
                        done = False
                        break
                if done:
                    val = resolve_expression(consts[c]["ln"], consts[c]["expression"], dep_values)
                    consts[c]["expression"] = None
                    consts[c]["dependencies"] = None
                    consts[c]["value"] = val
                    consts[c]["type"] = "int"
        if done:
            break
    if pass_num == 100 and not done:
        pass    # TODO: Print unresolved consts
    return consts


def generate_binary(sections):
    section_offsets = {}
    offsets = []
    for section_name in sections:
        offset = sections[section_name]["offset"]
        section_offsets[offset] = section_name
        offsets.append(offset)
    offsets.sort()
    sorted_sections = []
    for offset in offsets:
        section_name = section_offsets[offset]
        sorted_sections.append(section_name)
    output_binary = ["00" for _ in range(0x10000)]
    output_disassembly = []
    max_data_len = 0
    max_line_number_len = 0
    max_block_name_len = 0
    max_text_len = 0
    for section_name in sorted_sections:
        for block_name in sections[section_name]["blocks"]:
            if len(block_name) > max_block_name_len:
                max_block_name_len = len(block_name)
            for line in sections[section_name]["blocks"][block_name]["lines"]:
                if line["type"] != "bin":
                    raise Exception(f"Assembler error: Unprocessed line: {line}")
                data = line["data"]
                if len(data) > max_text_len:
                    max_text_len = len(data)
                ln = f'{line["ln"]}'
                if len(ln) > max_line_number_len:
                    max_line_number_len = len(ln)
                text = line["original_text"]
                if len(text) > max_data_len:
                    max_data_len = len(text)
    for section_name in sorted_sections:
        output_disassembly.append(f"=== Section {section_name}:")
        for block_name in sections[section_name]["blocks"]:
            block_name_added = False
            current_offset = sections[section_name]["blocks"][block_name]["offset"]
            for line in sections[section_name]["blocks"][block_name]["lines"]:
                if line["type"] != "bin":
                    raise Exception(f"Assembler error: Unprocessed line: {line}")
                offset = current_offset
                current_offset += line["size"]
                data = line["data"]
                ln = f'{line["ln"]}'
                original_text = line["original_text"]

                block_name_offset = ' ' * (max_block_name_len - len(block_name))
                data_append = ' ' * (max_data_len - len(data))
                ln_offset   = ' ' * (max_line_number_len - len(ln))
                chunks = [data[i : i + 2] for i in range(0, len(data), 2)]
                for i, chunk in enumerate(chunks):
                    output_binary[offset+i] = chunk
                if block_name_added:
                    output_disassembly.append(f"0x{offset:04x} {data}{data_append} | {ln_offset}{ln} {' ' * max_block_name_len}: {original_text}")
                else:
                    output_disassembly.append(f"0x{offset:04x} {data}{data_append} | {ln_offset}{ln} {block_name_offset}{block_name}: {original_text}")
                    block_name_added = True
    return output_binary, output_disassembly


def resolve_sections(constants, sections, opcode_processor):
    def _get_label_value(_label, _constants, _label_offsets, _current_offset):
        if _label == "$":
            return _current_offset
        if _label in _constants:
            return _constants[_label]["value"]
        elif _label in _label_offsets:
            return _label_offsets[_label]
        return None

    label_offsets = {}
    for _ in range(2):
        for section_name in sections:
            section = sections[section_name]
            current_offset = section["offset"]
            for block_name in section["blocks"]:
                block = section["blocks"][block_name]
                block["offset"] = current_offset
                if block_name not in label_offsets:
                    label_offsets[block_name] = current_offset
                for line in block["lines"]:
                    if line["type"] == "bin":
                        current_offset += line["size"]
                    elif line["type"] == "data":
                        line["offset"] = current_offset
                        current_offset += line["size"]
                        items = line["items"]
                        line_data = ""
                        line_resolved = True
                        for item in items:
                            if item[0] == "bin":
                                line_data += item[1]
                            elif item[0] == "label":
                                val = _get_label_value(item[2], constants, label_offsets, current_offset)
                                if val is None:
                                    line_resolved = False
                                    break
                                else:
                                    item[0] = "bin"
                                    item[1] = to_hex_with_zeros(val, 16)    # If it's a label and it's not in constants - it's a 16-bit address
                                    line_data += item[1]
                            elif item[0] == "expression":
                                line_resolved = False
                                break
                        if line_resolved:
                            line["data"] = line_data
                            line["type"] = "bin"
                            line["items"] = None
                    elif line["type"] == "opc":
                        # print(f"DEBUG::: Processing opcode {line}")
                        current_offset += line["size"]
                        deps = opcode_processor.get_dependencies(line)
                        deps_resolved = True
                        deps_dict = {}
                        for d in deps:
                            val = _get_label_value(d, constants, label_offsets, current_offset)
                            if val is None:
                                deps_resolved = False
                                break
                            else:
                                deps_dict[d] = val
                        if deps_resolved:
                            line["type"] = "bin"
                            line["data"] = opcode_processor.resolve_opcode(line, deps_dict)
                            line["args"] = None
    for section_name in sections:
        section = sections[section_name]
        for block_name in section["blocks"]:
            block = section["blocks"][block_name]
            block_size = 0
            for line in block["lines"]:
                if line["type"] == "bin":
                    block_size += line["size"]
            block["size"] = block_size


    return sections


def parse_lines(lines):
    constants = {}

    for line in lines:
        ln = line["line"]
        orig_text = line["text"]
        new_text = ""
        for c in orig_text: # Create a copy to keep original text unmodified
            new_text += c
        text_split = new_text.split(maxsplit=2)
        if len(text_split) == 3 and text_split[1] in ["EQU", "equ"]:  # Constants
            name, value = parse_constant(ln, text_split[0], text_split[2])
            if name not in constants:
                constants[name] = value
                constants[name]["ln"] = ln
            else:
                raise Exception(f"Syntax Error: Redefinition of {name} at line {ln}")
    constants = resolve_constants(constants)

    sections = {}
    current_section = ""
    current_label = ""
    opcode_processor = OpcodeProcessor(opcodes)
    for line in lines:
        ln = line["line"]
        orig_text = line["text"]
        new_text = ""
        for c in orig_text: # Create a copy to keep original text unmodified
            new_text += c
        text_split = new_text.split(maxsplit=2)
        if text_split[0].startswith("SECTION") or text_split[0].startswith("section"):  # Sections
            current_section = parse_section(ln, text_split, orig_text, sections)
        elif len(text_split) == 3 and text_split[1] in ["EQU", "equ"]:  # Constants
            pass    # Parsed on the previous step
        elif is_label(ln, text_split[0][:-1]) and text_split[0][-1] == ":": # Labels
            if current_section == "":
                raise Exception(f"Syntax error: Code block outside section (line {ln}: {line['text']})")
            label_name = text_split[0][:-1]
            if label_name in sections[current_section]["blocks"]:
                raise Exception(f"Error: Label redefinition at line {ln} (label {label_name})")
            # Python guarantees to preserve the order of dict items insertion:
            sections[current_section]["blocks"][label_name] = {"offset": -1, "size": -1, "binary": "", "lines": []}
            current_label = label_name
            if len(text_split) > 1:
                if text_split[1] in ["UINT8", "uint8", "UINT16", "uint16", "CHAR"]:
                    data = parse_data({"line": ln, "text":" ".join(text_split[1:])}, constants)
                    sections[current_section]["blocks"][current_label]["lines"].append(data)
                elif opcode_processor.is_opcode(text_split[1]):
                    opcode = opcode_processor.parse_opcode({"line": ln, "text":" ".join(text_split[1:])}, constants)
                    sections[current_section]["blocks"][current_label]["lines"].append(opcode)
                    pass
        elif opcode_processor.is_opcode(text_split[0]):
            if current_section == "":
                raise Exception(f"Syntax error: Code block outside section (line {ln}: {line['text']})")
            if current_label == "" or current_label not in sections[current_section]["blocks"]:
                raise Exception(f"Syntax error: Opcode outside code block (line {ln}: {line['text']})")
            opcode = opcode_processor.parse_opcode(line, constants)
            sections[current_section]["blocks"][current_label]["lines"].append(opcode)
        elif text_split[0] in ["UINT8", "uint8", "UINT16", "uint16", "CHAR"]:
            if current_section == "":
                raise Exception(f"Syntax error: Code block outside section (line {ln}: {line['text']})")
            if current_label == "":
                raise Exception(f"Syntax error: Opcode outside code block (line {ln}: {line['text']})")
            data = parse_data(line, constants)
            sections[current_section]["blocks"][current_label]["lines"].append(data)
        else:
            raise Exception(f"Syntax error: Garbage at line {ln}: {line['text']}")

    sections = resolve_sections(constants, sections, opcode_processor)
    return constants, sections

#==== Binary Generation ====#

def main(filename="test_new.asm"):
    with open(filename) as f:
        lines = f.read().split("\n")
    clean_lines = []
    for line_num, line in enumerate(lines):
        temp = line.split(";")[0].strip()  # Remove comments
        if len(temp) > 0:
            clean_lines.append({"line": line_num+1, "text": temp})
    lines = clean_lines
    constants, sections = parse_lines(lines)
    binary, disasm = generate_binary(sections)
    parsed = []
    for constant in constants:
        parsed.append(f"Constant {constant}: {constants[constant]}")
    for section_name in sections:
        parsed.append(f"Section {section_name} @{sections[section_name]["offset"]}:")
        for block_name in sections[section_name]["blocks"]:
            block = sections[section_name]["blocks"][block_name]
            parsed.append(f"\t{block_name}: offset: {block["offset"]} size: {block["size"]} binary:{block["binary"]}")
            for line in block["lines"]:
                parsed.append(f"\t\t{line}")
    with open("parsed_file_new.txt", 'w') as f:
        f.write("\n".join(parsed))
    with open("output_file.txt", 'w') as f:
        f.write("\n".join(disasm))

    parsed_bin = []
    for i in range(0, len(binary), 16):
        bin_string = f"0x{to_hex_with_zeros(i, 32)}: "
        bin_string += f"{binary[i]} {binary[i+1]} {binary[i+2]} {binary[i+3]} "
        bin_string += f"{binary[i+4]} {binary[i+5]} {binary[i+6]} {binary[i+7]}  "
        bin_string += f"{binary[i+8]} {binary[i+9]} {binary[i+10]} {binary[i+11]} "
        bin_string += f"{binary[i+12]} {binary[i+13]} {binary[i+14]} {binary[i+15]}"
        parsed_bin.append(bin_string)
    with open("binary_file.txt", 'w') as f:
        f.write("\n".join(parsed_bin))
    with open("output_file.bin", "wb") as file:
        # Convert hex string to actual bytes
        binary_data = bytes.fromhex("".join(binary))
        file.write(binary_data)


if __name__ == "__main__":
    main()