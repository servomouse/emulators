import re


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

registers = ["R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7", "F", "RA", "PC", "SP"]

# SRC/DST encoding:
arg_encoding = ["R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7", "F", "RA", "PC", "SP", "[RA]", "IMMED8", "IMMED16", "[IMMED16]"]

#==== Parsing ====#

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


def is_label(line_number, string):
    allowed_symbols = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_"
    digits = "0123456789"
    if string[0] not in allowed_symbols:
        return False
    for i in range(1, len(string)):
        if string[i] not in allowed_symbols+digits:
            return False
    if string in special_symbols:
        raise Exception(f"Syntax error: Invalid label at {line_number}: {string} (do not use special symbols as labels)")
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
        print(f"DEBUG::: {expr = }")
        if len(expr) == 3:
            if isinstance(expr[0], list):
                expr[0] = _prepare_expression(expr[0])
            if isinstance(expr[2], list):
                expr[2] = _prepare_expression(expr[2])
            return expr
        for _ in range(MAX_PASS_NUM):
            index = -1
            if isinstance(expr[0], list):
                print(f"DEBUG::: expr[0] is a list: {expr[0] = }, {expr[0][0] = }")
                expr[0] = _prepare_expression(expr[0])
                print(f"DEBUG::: After recursive call {expr = }")
            for i in range(1, len(expr), 2):
                if isinstance(expr[i+1], list):
                    print(f"DEBUG::: expr[i+1] is a list: {i = }, {expr[i+1] = }")
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
        print(f"DEBUG::: _parse_expression {expr = }")
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
        print(f"DEBUG::: _parse_expression {new_expr = }")
        return new_expr

    for op in math_operations+simbols:
        expression = expression.replace(op, f" {op} ")
    expression = expression.split()
    _labels = []
    for item in expression:
        if is_label(line_number, item) and item not in _labels:
            _labels.append(item)
    expression, _ = _parse_array(expression)
    print(f"DEBUG::: {expression = }")
    expression = _prepare_expression(expression)
    parsed_expr = _parse_expression(expression)
    return parsed_expr, _labels


def decode_arg(line_number, argument):
    arg = argument.strip()
    print(f"DEBUG::: Parsing argument {arg}")
    arg_type = ""
    arg_value = ""
    arg_code = 0
    arg_status = "pending"
    if arg in registers:
        arg_type = "REG"
        arg_value = arg
        arg_code = f"{arg_encoding.index(arg):x}"
        arg_status = "resolved"
    elif arg == "[RA]":
        arg_type = "[RA]"
        arg_value = "[RA]"
        arg_code = f"{arg_encoding.index(arg):x}"
        arg_status = "resolved"
    elif arg == "$":
        arg_type = "IMMED16"
        arg_value = "$"
        arg_code = f"{arg_encoding.index('[IMMED16]'):x}"
        arg_status = "pending"
    elif is_integer(arg):
        arg_value = get_integer_value(arg)
        if -127 < arg_value < 256:
            arg_type = "IMMED8"
            arg_code = f"{arg_encoding.index('IMMED8'):x}"
        elif arg_value < 0x10000:
            arg_type = "IMMED16"
            arg_code = f"{arg_encoding.index('IMMED16'):x}"
        else:
            raise Exception(f"Syntax error: Invalid argument at line {line_number}: {arg} (Value doesn't fit into 16 bit)")
        arg_status = "resolved"
    elif is_label(line_number, arg):
        arg_type = "label"  # May be either a constant (so, IMMED8 or IMED16) or an address
        arg_value = arg
        arg_status = "pending"
    elif arg[0] == "[" and arg[-1] == "]" and is_label(line_number, arg[1:-1]):
        arg_type = "[IMMED16]"
        arg_value = arg.replace("[", "").replace("]", "")
        arg_status = "pending"
    elif is_expression(line_number, arg):
        arg_type = "expression"
        arg_value = parse_expression(line_number, arg)
        arg_status = "pending"
    else:
        raise Exception(f"Syntax error: Invalid argument at line {line_number}: {arg}")
    return arg_type, arg_value, arg_code, arg_status


def parse_opcode(line):
    opcode_decoded = {"type": "opcode", "ln": line["line"], "opcode": "", "opcode_bin": "", "args": {"dst": None, "src": None}, "original_text": line["text"]}
    line_number = line["line"]
    substrs = line["text"].split(maxsplit=1)
    # print(f"Parsing line {line_number}, {substrs = }, {len(substrs) = }")
    opcode, args = (substrs[0], substrs[1]) if len(substrs) > 1 else (substrs[0], "")
    # print(f"Parsing line {line_number}, {opcode = }, {args = }")
    if opcode not in opcodes:
        raise Exception(f"Parsing error: Unknown opcode: {opcode} at line {line_number}")
    opcode_decoded["opcode_bin"] = opcodes[opcode]["bin"]
    opcode_decoded["opcode"] = opcode
    if opcode in ["NOP", "RET"]:
        # Opcodes without arguments
        pass
    elif opcode == "PUSH":
        # Opcodes with SRC only
        t, v, c, s = decode_arg(line_number, args)
        opcode_decoded["args"]["src"] = {'type': t, 'value': v, 'code': c, 'status': s}
    elif opcode in ["POP", "NOT", "JMP", "JPZ", "JNZ", "JPG", "JPL", "CALL", "INT", "IVT", "EI", "DI"]:
        # Opcodes with DST only
        t, v, c, s = decode_arg(line_number, args)
        opcode_decoded["args"]["dst"] = {'type': t, 'value': v, 'code': c, 'status': s}
    elif opcode in ["MOV", "SWAP", "AND", "OR", "XOR", "SFT", "ADD", "SUB", "CMP", "MUL", "DIV", "IN", "OUT"]:
        # Opcodes with two arguments
        dst, src = args.split(",", maxsplit=1)
        print(f"Parsing line {line_number}, {dst = }, {src = }")
        t, v, c, s = decode_arg(line_number, dst)
        opcode_decoded["args"]["dst"] = {'type': t, 'value': v, 'code': c, 'status': s}
        t, v, c, s = decode_arg(line_number, src)
        opcode_decoded["args"]["src"] = {'type': t, 'value': v, 'code': c, 'status': s}
    else:
        raise Exception(f"Unimplemented: Unparsed {opcode} at line {line_number}")
    return opcode_decoded


def parse_data(line):
    # print(f"DEBUG::: Parsing data {line[1]} at {line[0]}")
    line_number = line["line"]
    text = line["text"]
    def _parse_integer(string, num_bytes):
        string = string.strip()
        prefix = ""
        if string.startswith("0x") or string.startswith("0X"):
            prefix = "0x"
        elif string.startswith("0b"):
            prefix = "0x"
        if prefix != "":
            string = string[2:]
        for idx in range(len(string)):
            # print(f"DEBUG::: {idx = }, {string[idx] = }")
            if prefix == "" and string[idx] not in ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9"]:
                # print(f"DEBUG::: break 0")
                break
            elif prefix == "0x" and string[idx] not in ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A", 'a', "B", "b", "C", "c", "D", "d", "E", "e", "F", "f"]:
                # print(f"DEBUG::: break 1")
                break
            elif prefix == "0b" and string[idx] not in ["0", "1"]:
                # print(f"DEBUG::: break 2")
                break
        if idx == (len(string)-1):
            value = string
            reminder = ""
        else:
            value = string[:idx]
            reminder = string[idx:]
        value = prefix + value
        value = get_integer_value(value)
        if num_bytes == 1 and value > 0xFF:
            raise Exception(f"Value error: Value {hex(value)} is too large to fit into a byte! Line {line_number}")
        if num_bytes == 2 and value > 0xFFFF:
            raise Exception(f"Value error: Value {hex(value)} is too large to fit into a word! Line {line_number}")
        if reminder.startswith(","):
            reminder = reminder[1:]
        # print(f"DEBUG::: Returning value: {value} ({reminder = }, {idx = }, {prefix = }, {string[idx] = })")
        return value, reminder

    def _parse_string(string):
        string = string.strip()
        # print(f"DEBUG::: Parsing string {string}")
        # Starts with a " or '
        target = string[0]
        string = string[1:]     # Remove the first quotation mark
        end_found = False
        for i in range(1, len(string)):
            if string[i] == target:
                end_found = True
                break
        if not end_found:
            raise Exception(f"Syntax error: Couldn't find closing quotation mark ({target}), line {line_number}")
        value = string[:i]
        ascii_array = [ord(char) for char in value]
        if string[i+1] == ",":
            reminder = string[i+2:]
        else:
            reminder = string[i+1:]
        # print(f"DEBUG::: Returning value: {ascii_array} ({value = }, {reminder = })")
        return ascii_array, reminder

    def _parse_datastring(string, data_type):
        MAX_ITEMS_NUM = 1024
        string = string.strip()
        if data_type == "DB":
            num_bytes = 1
        elif data_type == "DW":
            num_bytes = 2
        values = []
        for _ in range(MAX_ITEMS_NUM):
            if string[0] in ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9"]:
                value, reminder = _parse_integer(string, num_bytes)
                values.append(value)
            elif string[0] in ["\"", "'"]:
                value, reminder = _parse_string(string)
                values += value
            else:
                return None
            if len(reminder) == 0:
                break
            string = reminder.strip()
        return values

    def _is_datastring(string, data_type):
        if _parse_datastring(string, data_type) is None:
            return False
        return True

    substrs = text.split(maxsplit=1)
    if len(substrs) < 2:
        raise Exception(f"Parsing error at line {line_number}: invalid data format: {text}")
    data_type, data = (substrs[0], substrs[1])

    if data_type == "DB":
        item_size = 8
    elif data_type == "DW":
        item_size = 16
    else:
        raise Exception(f"Syntax error: Unknown data format on line {line_number}: {text}")

    if _is_datastring(data, data_type):   # Integer, array of integers, or string
        data = _parse_datastring(data, data_type)
        num_items = len(data)
        status = "resolved"
        content = {"type": "data", "num_items": num_items, "item_size": item_size, "values": data}
    elif is_label(line_number, data):
        status = "pending"
        content = {"type": "label", "num_items": 1, "item_size": item_size, "label": data}
    elif is_expression(line_number, data):
        expr, labels = parse_expression(line_number, data)
        status = "pending"
        content = {"type": "expression", "num_items": 1, "item_size": item_size, "expression": expr, "dependencies": labels}
    else:
        raise Exception(f"Syntax error: Unknown data type at line {line_number}: {text}")
    return {"type": "data", "ln": line_number, "content": content, "original_text": text, "status": status}


def parse_constant(line_number, name, expression):
    if is_integer(expression):
        value = get_integer_value(expression)
        status = "resolved"
        expr_type = "INT"
    elif is_label(line_number, expression):
        value = expression
        status = "pending"
        expr_type = "LABEL"
    elif is_expression(line_number, expression):
        expr, labels = parse_expression(line_number, expression)
        value = {"expression": expr, "labels": labels}
        status = "pending"
        expr_type = "EXPRESSION"
    else:
        raise Exception(f"Parsing error: Invalid expression on line {line_number}: {expression}")
    return name, {"type": expr_type, "value": value, "status": status}


def parse_section(ln, text_split, text, sections):
    if len(text_split) == 2:
        section_name = text_split[1]
        if not is_label(ln, section_name):
            raise Exception(f"Syntax error: Invalid section name at {ln}: {section_name}")
        if section_name in sections:
            current_section = section_name
        else:
            raise Exception(f"Error: Line {ln}: Using section {section_name} before specifying its offset! Set the offset like SECTION section_name, 0xABCD")
    elif len(text_split) == 3:
        section_name = text_split[1]
        if not is_label(ln, section_name):
            raise Exception(f"Syntax error: Invalid section name at {ln}: {section_name}")
        current_section = section_name
        section_offset = text_split[2]
        section_offset_type = ""
        if is_integer(section_offset):
            section_offset = get_integer_value(section_offset)
            section_offset_type = "INT"
        elif is_label(ln, section_offset):
            section_offset_type = "LABEL"
        elif is_expression(ln, section_offset):
            section_offset_type = "EXPRESSION"
        else:
            raise Exception(f"Syntax error: Invalid section offset definition at {ln}: {text}")
        if section_name in sections:
            raise Exception(f"Syntax error: Invalid section offset redefinition at {ln}: {text}")
        sections[section_name] = {"offset": section_offset, "blocks": {}, "status": "pending"}
    return current_section


#==== Resolving ====#


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
    MAX_PASSES_NUM = 100
    for pass_num in range(MAX_PASSES_NUM):
        done = True
        for c in consts:
            if consts[c]["status"] == "resolved":
                continue
            if consts[c]["type"] == "LABEL":
                c_name = consts[c]["value"]
                if consts[c_name]["status"] == "resolved":
                    consts[c]["value"] = consts[c_name]["value"]
                    consts[c]["status"] = "resolved"
                    consts[c]["type"] = consts[c_name]["type"]
                else:
                    done = False
            elif consts[c]["type"] == "EXPRESSION":
                dependensies = consts[c]["value"]["labels"]
                dep_values = {}
                for d in dependensies:
                    if consts[d]["status"] == "resolved":
                        dep_values[d] = consts[d]["value"]
                    else:
                        done = False
                        break
                if done:
                    val = resolve_expression(consts[c]["LN"], consts[c]["value"]["expression"], dep_values)
                    consts[c]["value"] = val
                    consts[c]["status"] = "resolved"
                    consts[c]["type"] = "INT"
        if done:
            break
    if pass_num == 100:
        pass    # TODO: Print unresolved consts
    return consts


def proces_data_line(line, constants, label_offsets, current_offset):
    line_size = 0
    line_binary = ""
    if line["content"]["type"] == "data":
        item_size = line["content"]["item_size"]
        for v in line["content"]["values"]:
            line_binary += to_hex_with_zeros(v, item_size)
        line["type"] = "bin"
        line["offset"] = current_offset
        line["content"] = line_binary
        line["size"] = len(line_binary)//2
        line_size = line["size"]
    elif line["content"]["type"] == "label":
        item_size = line["content"]["item_size"]
        num_items = line["content"]["num_items"]
        label = line["content"]["label"]
        if label in constants:
            v = constants[label]["value"]
        elif label in label_offsets:
            v = label_offsets[label]
        else:
            v = None
            line_size = num_items * (item_size // 8)

        if v is not None:
            line_binary += to_hex_with_zeros(v, item_size)
            line["type"] = "bin"
            line["offset"] = current_offset
            line["content"] = line_binary
            line["size"] = len(line_binary)//2
            line_size = line["size"]
    elif line["content"]["type"] == "expression":
        item_size = line["content"]["item_size"]
        num_items = line["content"]["num_items"]
        expression = line["content"]["expression"]
        dependencies = line["content"]["dependencies"]
        deps_dict = {}
        resolved = True
        for d in dependencies:
            if d in constants:
                deps_dict[d] = constants[d]["value"]
            elif d in label_offsets:
                deps_dict[d] = label_offsets[d]
            else:
                resolved = False
        if resolved:
            v = resolve_expression(line["ln"], expression, deps_dict)
            line_binary += to_hex_with_zeros(v, item_size)
            line["type"] = "bin"
            line["offset"] = current_offset
            line["content"] = line_binary
            line["size"] = len(line_binary)//2
            line_size = line["size"]
    return line_size


def proces_opcode_line(line, constants, label_offsets, current_offset):
    line_size = 1
    opcode = line["opcode"]
    opcode_binary = opcodes[opcode]["bin"]
    line_binary = ""
    dst_arg = line["args"]["dst"]
    src_arg = line["args"]["src"]
    resolved = True
    if dst_arg is not None:
        line_size = 2
        if dst_arg["type"] in ["REG", "[RA]"]:
            opcode_binary += to_hex_with_zeros(arg_encoding.index(dst_arg["value"]), 4)
        elif dst_arg["type"] in ["IMMED8", "IMMED16", "[IMMED16]"]:
            if dst_arg["value"] == "$":
                opcode_binary += to_hex_with_zeros(arg_encoding.index("[IMMED16]"), 4)
                line_binary += to_hex_with_zeros(current_offset, 16)
                line_size += 2
            elif dst_arg["type"] in ["IMMED8", "IMMED16"]:
                opcode_binary += to_hex_with_zeros(arg_encoding.index(dst_arg["type"]), 4)
                if dst_arg["type"] == "IMMED8":
                    line_binary += to_hex_with_zeros(dst_arg["value"], 8)
                    line_size += 1
                else:
                    line_binary += to_hex_with_zeros(dst_arg["value"], 16)
                    line_size += 2
            elif dst_arg["type"] == "[IMMED16]":
                opcode_binary += to_hex_with_zeros(arg_encoding.index("[IMMED16]"), 4)
                label = dst_arg["value"]
                if label in constants:
                    val = constants[label]["value"]
                    line_binary += to_hex_with_zeros(val, 16)
                    line_size += 2
                elif label in label_offsets:
                    val = label_offsets[label]
                    line_binary += to_hex_with_zeros(val, 16)
                    line_size += 2
                else:
                    line_size += 2
                    resolved = False
        elif dst_arg["type"] == "label":
            label = dst_arg["value"]
            if label in constants:
                val = constants[label]["value"]
                if val > 0xFF:
                    opcode_binary += to_hex_with_zeros(arg_encoding.index("IMMED16"), 4)
                    line_binary += to_hex_with_zeros(val, 16)
                    line_size += 2
                else:
                    opcode_binary += to_hex_with_zeros(arg_encoding.index("IMMED8"), 4)
                    line_binary += to_hex_with_zeros(val, 8)
                    line_size += 1
            elif label in label_offsets:
                opcode_binary += to_hex_with_zeros(arg_encoding.index("IMMED16"), 4)
                val = label_offsets[label]
                line_binary += to_hex_with_zeros(val, 16)
                line_size += 2
            else:
                line_size += 2
                resolved = False
        elif dst_arg["type"] == "expression":
            expression = dst_arg["expression"]
            deps_list = dst_arg["dependencies"]
            deps_dict = {}
            expr_resolved = True
            for d in deps_list:
                if d in constants:
                    deps_dict[d] = constants[d]["value"]
                elif d in label_offsets:
                    deps_dict[d] = label_offsets[d]
                else:
                    expr_resolved = False
            if expr_resolved:
                val = resolve_expression(line["ln"], expression, deps_dict)
                if val > 0xFF:
                    opcode_binary += to_hex_with_zeros(arg_encoding.index("IMMED16"), 4)
                    line_binary += to_hex_with_zeros(val, 16)
                    line_size += 2
                else:
                    opcode_binary += to_hex_with_zeros(arg_encoding.index("IMMED8"), 4)
                    line_binary += to_hex_with_zeros(val, 8)
                    line_size += 1
            else:
                resolved = False
    else:
        if src_arg is not None:
            opcode_binary += "0"
    if src_arg is not None:
        if line_size == 1:
            line_size = 2
        if src_arg["type"] in ["REG", "[RA]"]:
            opcode_binary += to_hex_with_zeros(arg_encoding.index(src_arg["value"]), 4)
        elif src_arg["type"] in ["IMMED8", "IMMED16", "[IMMED16]"]:
            if src_arg["value"] == "$":
                opcode_binary += to_hex_with_zeros(arg_encoding.index("[IMMED16]"), 4)
                line_binary += to_hex_with_zeros(current_offset, 16)
                line_size += 2
            elif src_arg["type"] in ["IMMED8", "IMMED16"]:
                opcode_binary += to_hex_with_zeros(arg_encoding.index(src_arg["type"]), 4)
                if src_arg["type"] == "IMMED8":
                    line_binary += to_hex_with_zeros(src_arg["value"], 8)
                    line_size += 1
                else:
                    line_binary += to_hex_with_zeros(src_arg["value"], 16)
                    line_size += 2
            elif src_arg["type"] == "[IMMED16]":
                opcode_binary += to_hex_with_zeros(arg_encoding.index("[IMMED16]"), 4)
                label = src_arg["value"]
                if label in constants:
                    val = constants[label]["value"]
                    line_binary += to_hex_with_zeros(val, 16)
                    line_size += 2
                elif label in label_offsets:
                    val = label_offsets[label]
                    line_binary += to_hex_with_zeros(val, 16)
                    line_size += 2
                else:
                    line_size += 2
                    resolved = False
        elif src_arg["type"] == "label":
            label = src_arg["value"]
            if label in constants:
                val = constants[label]["value"]
                if val > 0xFF:
                    opcode_binary += to_hex_with_zeros(arg_encoding.index("IMMED16"), 4)
                    line_binary += to_hex_with_zeros(val, 16)
                    line_size += 2
                else:
                    opcode_binary += to_hex_with_zeros(arg_encoding.index("IMMED8"), 4)
                    line_binary += to_hex_with_zeros(val, 8)
                    line_size += 1
            elif label in label_offsets:
                opcode_binary += to_hex_with_zeros(arg_encoding.index("IMMED16"), 4)
                val = label_offsets[label]
                line_binary += to_hex_with_zeros(val, 16)
                line_size += 2
            else:
                line_size += 2
                resolved = False
        elif src_arg["type"] == "expression":
            expression = src_arg["value"][0]
            deps_list = src_arg["value"][1]
            deps_dict = {}
            expr_resolved = True
            for d in deps_list:
                if d in constants:
                    deps_dict[d] = constants[d]["value"]
                elif d in label_offsets:
                    deps_dict[d] = label_offsets[d]
                else:
                    expr_resolved = False
            if expr_resolved:
                val = resolve_expression(line["ln"], expression, deps_dict)
                if val > 0xFF:
                    opcode_binary += to_hex_with_zeros(arg_encoding.index("IMMED16"), 4)
                    line_binary += to_hex_with_zeros(val, 16)
                    line_size += 2
                else:
                    opcode_binary += to_hex_with_zeros(arg_encoding.index("IMMED8"), 4)
                    line_binary += to_hex_with_zeros(val, 8)
                    line_size += 1
            else:
                resolved = False
    else:
        if dst_arg is not None:
            opcode_binary += "0"
    final_binary = opcode_binary + line_binary
    print(f"DEBUG::: {line['original_text']}, => 0x{current_offset:04x}: 0x{final_binary}")
    if resolved:
        line["type"] = "bin"
        line["offset"] = current_offset
        line["content"] = final_binary
        line["size"] = len(final_binary)//2
        # line["args"] = None
    return line_size


def resolve_sections(constants, sections):
    label_offsets = {}
    for _ in range(2):
        for section_name in sections:
            current_offset = sections[section_name]["offset"]
            for block_name in sections[section_name]["blocks"]:
                if block_name not in label_offsets:
                    label_offsets[block_name] = current_offset
                if sections[section_name]["blocks"][block_name]["status"] == "resolved":
                    continue
                sections[section_name]["blocks"][block_name]["offset"] = current_offset
                for line in sections[section_name]["blocks"][block_name]["lines"]:
                    if line["type"] == "data":
                        current_offset += proces_data_line(line, constants, label_offsets, current_offset)
                    elif line["type"] == "opcode":
                        current_offset += proces_opcode_line(line, constants, label_offsets, current_offset)
                    elif line["type"] == "bin":
                        current_offset += line["size"]
    for section_name in sections:
        current_offset = sections[section_name]["offset"]
        for block_name in sections[section_name]["blocks"]:
            block_binary = ""
            for line in sections[section_name]["blocks"][block_name]["lines"]:
                if line["type"] == "bin":
                    block_binary += line["content"]
                else:
                    raise Exception(f"Assembler error: Unprocessed line: {line}")
            block_size = len(block_binary)//2
            sections[section_name]["blocks"][block_name]["binary"] = block_binary
            sections[section_name]["blocks"][block_name]["size"] = block_size
            sections[section_name]["blocks"][block_name]["status"] = "resolved"
            current_offset += block_size
    return sections


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
                data = line["content"]
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
            for line in sections[section_name]["blocks"][block_name]["lines"]:
                if line["type"] != "bin":
                    raise Exception(f"Assembler error: Unprocessed line: {line}")
                offset = line["offset"]
                data = line["content"]
                ln = f'{line["ln"]}'
                original_text = line["original_text"]

                block_name_offset = ' ' * (max_block_name_len - len(block_name))
                data_append = ' ' * (max_data_len - len(data))
                ln_offset   = ' ' * (max_line_number_len - len(ln))
                # if offset in output_binary:
                #     raise Exception(f"Linking error: Overlapping code at line {ln}: {original_text}")
                # output_binary[offset] = data
                chunks = [data[i : i + 2] for i in range(0, len(data), 2)]
                for i, chunk in enumerate(chunks):
                    output_binary[offset+i] = chunk
                if block_name_added:
                    output_disassembly.append(f"0x{offset:04x} {data}{data_append} | {ln_offset}{ln} {' ' * max_block_name_len}: {original_text}")
                else:
                    output_disassembly.append(f"0x{offset:04x} {data}{data_append} | {ln_offset}{ln} {block_name_offset}{block_name}: {original_text}")
                    block_name_added = True
    return output_binary, output_disassembly


def parse_lines(lines):
    sections = {}
    constants = {}
    current_section = ""
    current_label = ""
    for line in lines:
        ln = line["line"]
        text = line["text"].replace(",", " ").split(maxsplit=2)
        text_split = line["text"].replace(",", " ").split(maxsplit=2)
        if text_split[0].startswith("SECTION") or text_split[0].startswith("section"):
            current_section = parse_section(ln, text_split, text, sections)
        elif len(text_split) == 3 and text_split[1] in ["EQU", "equ"]:
            name, value = parse_constant(ln, text_split[0], text_split[2])
            if name not in constants:
                constants[name] = value
                constants[name]["LN"] = ln
            else:
                raise Exception(f"Syntax Error: Redefinition of {name} at line {ln}")
        elif is_label(ln, text_split[0][:-1]) and text_split[0][-1] == ":":
            if current_section == "":
                raise Exception(f"Syntax error: Code block outside section (line {ln}: {line['text']})")
            label_name = text_split[0][:-1]
            # Python guarantees to preserve the order of dict items insertion:
            sections[current_section]["blocks"][label_name] = {"offset": -1, "size": -1, "binary": "", "lines": [], "status": "pending"}
            current_label = label_name
            if len(text_split) > 1:
                if text_split[1] in opcodes:
                    opcode = parse_opcode([ln, " ".join(text_split[1:])])
                    sections[current_section]["blocks"][current_label]["lines"].append(opcode)
                elif text_split[1] in ["DB", "db", "DW", "dw"]:
                    data = parse_data({"line": ln, "text":" ".join(text_split[1:])})
                    sections[current_section]["blocks"][current_label]["lines"].append(data)
        elif text_split[0] in opcodes:
            if current_section == "":
                raise Exception(f"Syntax error: Code block outside section (line {ln}: {line['text']})")
            if current_label == "":
                raise Exception(f"Syntax error: Opcode outside code block (line {ln}: {line['text']})")
            opcode = parse_opcode(line)
            sections[current_section]["blocks"][current_label]["lines"].append(opcode)
        elif text_split[0] in ["DB", "db", "DW", "dw"]:
            if current_section == "":
                raise Exception(f"Syntax error: Code block outside section (line {ln}: {line['text']})")
            if current_label == "":
                raise Exception(f"Syntax error: Opcode outside code block (line {ln}: {line['text']})")
            data = parse_data(line)
            sections[current_section]["blocks"][current_label]["lines"].append(data)
        else:
            raise Exception(f"Syntax error: Garbage at line {ln}: {line['text']}")

    constants = resolve_constants(constants)
    sections = resolve_sections(constants, sections)
    return constants, sections

#==== Binary Generation ====#

def encode_arg(arg_dict, arg_type, opcode, constants, label_offsets, ln, current_offset):
    if arg_dict is None:
        return "0", ""
    if arg_dict["type"] in ["REG", "[RA]", "IMMED8", "IMMED16"]:
        arg_value = arg_dict["value"]
        if isinstance(arg_value, int):
            if arg_dict["type"] == "IMMED16":
                arg_value = to_hex_with_zeros(arg_value, 16)
            else:
                arg_value = to_hex_with_zeros(arg_value, 8)
        return arg_dict["code"], arg_value
    elif arg_dict["type"] == "label":
        arg_value = arg_dict["value"]
        arg_code = "0"
        if arg_value in constants:
            arg_value = constants[arg_value]["value"]
            if arg_value < 0xFF:
                arg_code = to_hex_with_zeros(arg_encoding.index("IMMED8"), 8)[-1]
                arg_value = to_hex_with_zeros(arg_value, 8)
            else:
                arg_code = to_hex_with_zeros(arg_encoding.index("IMMED16"), 8)[-1]
                arg_value = to_hex_with_zeros(arg_value, 16)
        elif arg_value in label_offsets:
            arg_value = to_hex_with_zeros(label_offsets[arg_value], 16)
            arg_code = to_hex_with_zeros(arg_encoding.index("[IMMED16]"), 8)[-1]
        else:
            arg_value = "____"
            arg_code = to_hex_with_zeros(arg_encoding.index("[IMMED16]"), 8)[-1]
        return arg_code, arg_value
    else:
        return "0", ""

# def generate_binary(constants, sections):

#     def _process_data_line(line, constants, label_offsets, current_offset):
#         # Example:
#         # {'type': 'data', 'ln': 26, 'num_items': 3, 'item_size': 2, 'values': [291, 9029, 13398], 
#         #  'original_text': 'DW 0x0123 0x2345 0x3456', 'status': 'resolved'}
#         line_addr = current_offset
#         line_number = line["ln"]
#         line_text = line["original_text"]
#         line_length = 0
#         line_binary = ""
#         line_content = line["content"]
#         item_size = line_content["item_size"]
#         if line_content["type"] == "data":
#             line_values = line_content["values"]
#             item_size = line_content["item_size"]
#             for val in line_values:
#                 if item_size == 1:
#                     line_binary += f"{val:02x}"
#                 elif item_size == 2:
#                     line_binary += f"{val:04x}"
#                 line_length += item_size * line_content["num_items"]
#         elif line_content["type"] == "label":
#             label = line_content["label"]
#             if label in constants:
#                 val = constants[label]["value"]
#                 if item_size == 1:
#                     val &= 0xFF
#                     line_binary += f"{val:02x}"
#                 elif item_size == 2:
#                     val &= 0xFFFF
#                     line_binary += f"{val:04x}"
#                 line_content["values"] = [val]
#                 line["status"] == "resolved"
#             elif label in label_offsets:
#                 val = label_offsets[label]
#                 if item_size == 1:
#                     val &= 0xFF
#                     line_binary += f"{val:02x}"
#                 elif item_size == 2:
#                     val &= 0xFFFF
#                     line_binary += f"{val:04x}"
#                 line_content["values"] = [val]
#                 line["status"] == "resolved"
#                 line_content["type"] = "data"
#             else:
#                 line_binary += "__" * item_size
#             line_length += item_size * line_content["num_items"]
#         elif line_content["type"] == "expression":
#             expression = line_content["expression"]
#             dependensies = line_content["dependencies"]
#             resolved_deps = {}
#             resolved = True
#             for dep in dependensies:
#                 if dep in constants:
#                     resolved_deps[dep] = constants[dep]["value"]
#                 elif dep in label_offsets:
#                     resolved_deps[dep] = label_offsets[dep]
#                 else:
#                     resolved = False
#                     break
#             if resolved:
#                 val = resolve_expression(line_number, expression, resolved_deps)
#                 if item_size == 1:
#                     val &= 0xFF
#                     line_binary += f"{val:02x}"
#                 elif item_size == 2:
#                     val &= 0xFFFF
#                     line_binary += f"{val:04x}"
#                 line_content["values"] = [val]
#                 line_content["type"] = "data"
#                 line["status"] == "resolved"
#             else:
#                 line_binary += "__" * item_size
#             line_length += item_size * line_content["num_items"]
#         return line_addr, line_binary, line_length, line_number, line_text

#     new_sections = {}
#     label_offsets = {}
#     # First run to resolve symbols:
#     for section_name in sections:
#         current_offset = sections[section_name]["offset"]["value"]
#         blocks = sections[section_name]["blocks"]
#         section_data = []   # Offset - Label - Binary - Line number - Original string
#         for block_name in blocks:
#             blocks[block_name]["offset"] = current_offset
#             label_offsets[block_name] = current_offset
#             lines = blocks[block_name]["lines"]
#             block_name_added = False
#             for line in lines:
#                 if line["type"] == "data":
#                     line_addr, line_binary, line_length, line_number, line_text = _process_data_line(line, constants, label_offsets, current_offset)
#                     current_offset += line_length
#                 if line["type"] == "opcode":
#                     # Example: 
#                     # {'type': 'opcode', 'ln': 39, 'opcode': 'MOV', 'opcode_bin': '01', 'args': {
#                     #     'dst': {'type': 'REG', 'value': '', 'code': 'b', 'status': 'resolved'}, 
#                     #     'src': {'type': 'LABEL', 'value': 'STACK_INIT', 'code': 0, 'status': 'pending'}
#                     # }, 'original_text': 'MOV SP, STACK_INIT'}
#                     line_addr = current_offset
#                     line_binary = line["opcode_bin"]
#                     args = line["args"]
#                     dst_code, dst_val, src_code, src_val = "0", "", "0", ""
#                     dst_code, dst_val = encode_arg(args["dst"], "dst", line["opcode"], constants, label_offsets, line["ln"], current_offset)
#                     src_code, src_val = encode_arg(args["src"], "src", line["opcode"], constants, label_offsets, line["ln"], current_offset)
#                     line_binary += dst_code + src_code + dst_val + src_val
#                     current_offset += len(line_binary)//2
#     # Second run to construct the binary
#     for section_name in sections:
#         current_offset = sections[section_name]["offset"]["value"]
#         blocks = sections[section_name]["blocks"]
#         section_data = []   # Offset - Label - Binary - Line number - Original string
#         for block_name in blocks:
#             blocks[block_name]["offset"] = current_offset
#             label_offsets[block_name] = current_offset
#             lines = blocks[block_name]["lines"]
#             block_name_added = False
#             for line in lines:
#                 if line["type"] == "data":
#                     line_addr, line_binary, line_length, line_number, line_text = _process_data_line(line, constants, label_offsets, current_offset)
#                     current_offset += line_length
#                     if not block_name_added:
#                         section_data.append({"offset": line_addr, "binary": line_binary, "label": block_name, "ln": line_number, "original_text": line_text})
#                         block_name_added = True
#                     else:
#                         section_data.append({"offset": line_addr, "binary": line_binary, "label": "", "ln": line_number, "original_text": line_text})
#                 if line["type"] == "opcode":
#                     # Example: 
#                     # {'type': 'opcode', 'ln': 39, 'opcode': 'MOV', 'opcode_bin': '01', 'args': {
#                     #     'dst': {'type': 'REG', 'value': '', 'code': 'b', 'status': 'resolved'}, 
#                     #     'src': {'type': 'LABEL', 'value': 'STACK_INIT', 'code': 0, 'status': 'pending'}
#                     # }, 'original_text': 'MOV SP, STACK_INIT'}
#                     line_addr = current_offset
#                     line_binary = line["opcode_bin"]
#                     args = line["args"]
#                     dst_code, dst_val, src_code, src_val = "0", "", "0", ""
#                     dst_code, dst_val = encode_arg(args["dst"], "dst", line["opcode"], constants, label_offsets, line["ln"], current_offset)
#                     src_code, src_val = encode_arg(args["src"], "src", line["opcode"], constants, label_offsets, line["ln"], current_offset)
#                     line_binary += dst_code + src_code + dst_val + src_val
#                     current_offset += len(line_binary)//2
#                     if not block_name_added:
#                         section_data.append({"offset": line_addr, "binary": line_binary, "label": block_name, "ln": line["ln"], "original_text": line["original_text"]})
#                         block_name_added = True
#                     else:
#                         section_data.append({"offset": line_addr, "binary": line_binary, "label": "", "ln": line["ln"], "original_text": line["original_text"]})
#         # Align the fields:
#         max_bin_len = 0
#         max_label_len = 0
#         for item in section_data:
#             bin_len = len(item["binary"])
#             label_len = len(item["label"])
#             if bin_len > max_bin_len:
#                 max_bin_len = bin_len
#             if label_len > max_label_len:
#                 max_label_len = label_len
#         for item in section_data:
#             item["binary"] += " "*(max_bin_len-len(item["binary"]))
#             item["label"] += " "*(max_label_len-len(item["label"]))
#         new_sections[section_name] = section_data
#     return new_sections


def main(filename="test.asm"):
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
    # new_sections = generate_binary(constants, sections)
    parsed = []
    for constant in constants:
        parsed.append(f"Constant {constant}: {constants[constant]}")
    for section in sections:
        parsed.append(f"Section {section}: {sections[section]}")
    with open("parsed_file.txt", 'w') as f:
        f.write("\n".join(parsed))
    with open("output_file.txt", 'w') as f:
        f.write("\n".join(disasm))

    parsed_bin = []
    for i in range(0, len(binary), 16):
        bin_string = f"0x{to_hex_with_zeros(i, 16)}: "
        bin_string += f"{binary[i]} {binary[i+1]} {binary[i+2]} {binary[i+3]}  "
        bin_string += f"{binary[i+4]} {binary[i+5]} {binary[i+6]} {binary[+7]}"
        parsed_bin.append(bin_string)
    with open("binary_file.txt", 'w') as f:
        f.write("\n".join(parsed_bin))
    # output = []
    # for sec_name in new_sections:
    #     section = new_sections[sec_name]
    #     output.append(f"Section {sec_name}:")
    #     # {'offset': 393, 'binary': '00', 'label': '', 'ln': 134, 'original_text': 'NOP'}
    #     for line in section:
    #         output.append(f"0x{line['offset']:04x} 0x{line['binary']} {line['ln']} {line['label']} {line['original_text']}")
    # with open("output_file.txt", 'w') as f:
    #     f.write("\n".join(output))


if __name__ == "__main__":
    main()