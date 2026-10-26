import re
import json

opcodes = {
    "NOP":  {"bin": "00", "dst": None, "src": None},
    "MOV":  {"bin": "01", "dst": ["R", "RA", "(RA)", "ADDR"], "src": ["R", "RA", "(RA)", "ADDR", "IMMED8"]},
    "PUSH": {"bin": "02", "dst": None, "src": ["R", "RA", "(RA)", "ADDR", "IMMED8"]},
    "POP":  {"bin": "03", "dst": ["R", "RA", "(RA)", "ADDR"], "src": None},
    "AND":  {"bin": "04", "dst": ["R", "RA", "(RA)", "ADDR"], "src": ["R", "RA", "(RA)", "ADDR", "IMMED8"]},
    "OR":   {"bin": "05", "dst": ["R", "RA", "(RA)", "ADDR"], "src": ["R", "RA", "(RA)", "ADDR", "IMMED8"]},
    "XOR":  {"bin": "06", "dst": ["R", "RA", "(RA)", "ADDR"], "src": ["R", "RA", "(RA)", "ADDR", "IMMED8"]},
    "NOT":  {"bin": "07", "dst": ["R", "RA", "(RA)", "ADDR"], "src": None},
    "SFT":  {"bin": "08", "dst": ["R", "RA", "(RA)", "ADDR"], "src": ["R", "RA", "(RA)", "ADDR", "IMMED8"]},
    "ADD":  {"bin": "09", "dst": ["R", "RA", "(RA)", "ADDR"], "src": ["R", "RA", "(RA)", "ADDR", "IMMED8"]},
    "SUB":  {"bin": "0A", "dst": ["R", "RA", "(RA)", "ADDR"], "src": ["R", "RA", "(RA)", "ADDR", "IMMED8"]},
    "CMP":  {"bin": "0B", "dst": ["R", "RA", "(RA)", "ADDR"], "src": ["R", "RA", "(RA)", "ADDR", "IMMED8"]},
    "MUL":  {"bin": "0C", "dst": ["R", "RA", "(RA)", "ADDR"], "src": ["R", "RA", "(RA)", "ADDR", "IMMED8"]},
    "DIV":  {"bin": "0D", "dst": ["R", "RA", "(RA)", "ADDR"], "src": ["R", "RA", "(RA)", "ADDR", "IMMED8"]},
    "JMP":  {"bin": "0E", "dst": ["R", "(RA)", "ADDR", "IMMED8", "IMMED16"], "src": None},
    "JPZ":  {"bin": "0F", "dst": ["R", "(RA)", "ADDR", "IMMED8", "IMMED16"], "src": None},
    "JNZ":  {"bin": "10", "dst": ["R", "(RA)", "ADDR", "IMMED8", "IMMED16"], "src": None},
    "JPG":  {"bin": "11", "dst": ["R", "(RA)", "ADDR", "IMMED8", "IMMED16"], "src": None},
    "JPL":  {"bin": "12", "dst": ["R", "(RA)", "ADDR", "IMMED8", "IMMED16"], "src": None},
    "CALL": {"bin": "13", "dst": ["(RA)", "ADDR"], "src": None},
    "RET":  {"bin": "14", "dst": None, "src": None},
    "IN":   {"bin": "15", "dst": ["R", "RA", "(RA)", "ADDR"], "src": ["R", "IMMED8"]}, # SRC is a port
    "OUT":  {"bin": "16", "dst": ["R", "IMMED8"], "src": ["R", "RA", "(RA)", "ADDR"]}, # DST is a port
    "INT":  {"bin": "17", "dst": ["R", "IMMED8"], "src": None},
    "IVT":  {"bin": "18", "dst": ["IMMED16"], "src": None},
    "EI":   {"bin": "19", "dst": ["R", "IMMED8"], "src": None},
    "DI":   {"bin": "1A", "dst": ["R", "IMMED8"], "src": None},
}

registers = ["R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7", "F", "RA", "PC", "SP"]

# SRC/DST encoding:
arg_encoding = ["R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7", "F", "RA", "PC", "SP", "IMMED8", "IMMED16", "ADDR", "(RA)"]


def is_integer(s):
    try:
        int(s.strip(), base=0)
        return True
    except ValueError:
        return False


def get_int_value(s):
    return int(s.strip(), base=0)


def decode_arg(argument):
    arg = argument.strip()
    arg_type = ""
    arg_value = ""
    arg_status = "pending"
    if arg in registers:
        arg_type = arg
        arg_value = f"{registers.index(arg):02}"
        arg_status = "resolved"
    elif arg == "(RA)":
        arg_type = arg
        arg_value = f"{arg_encoding.index(arg):02}"
        arg_status = "resolved"
    elif is_integer(arg):
        arg_value = get_int_value(arg)
        if -127 < arg_value < 256:
            arg_type = "IMMED8"
        elif arg_value < 0x10000:
            arg_type = "IMMED16"
        else:
            arg_type = "INVALID ARGUMENT"
        arg_status = "resolved"
    elif is_label(arg):
        arg_type = "LABEL"
        arg_value = arg
        arg_status = "pending"
    elif is_label(arg, as_array=True):
        arg_type = "EXPRESSION"
        arg_value = split_label_expression(arg)
        arg_status = "pending"
    return arg_type, arg_value, arg_status


def parse_opcode(line):
    opcode_decoded = {"opcode": "", "args": []}
    line_number = line[0]
    substrs = line[1].split(maxsplit=1)
    # print(f"Parsing line {line_number}, {substrs = }, {len(substrs) = }")
    opcode, args = (substrs[0], substrs[1]) if len(substrs) > 1 else (substrs[0], "")
    # print(f"Parsing line {line_number}, {opcode = }, {args = }")
    if opcode in ["NOP", "RET"]:
        # Opcodes without arguments
        opcode_decoded["opcode"] = opcodes[opcode]["bin"]
    elif opcode == "PUSH":
        # Opcodes with SRC only
        opcode_decoded["opcode"] = opcodes[opcode]["bin"]
        t, v, s = decode_arg(args)
        opcode_decoded["args"].append({'type': t, 'value': v, 'status': s})   # SRC
    elif opcode in ["POP", "NOT", "JMP", "JPZ", "JNZ", "JPG", "JPL", "CALL", "INT", "IVT", "EI", "DI"]:
        # Opcodes with DST only
        opcode_decoded["opcode"] = opcodes[opcode]["bin"]
        t, v, s = decode_arg(args)
        opcode_decoded["args"].append({'type': t, 'value': v, 'status': s})   # DST
    elif opcode in ["MOV", "AND", "OR", "XOR", "SFT", "ADD", "SUB", "CMP", "MUL", "DIV", "IN", "OUT"]:
        # Opcodes with two arguments
        opcode_decoded["opcode"] = opcodes[opcode]["bin"]
        dst, src = args.split(",", maxsplit=1)
        print(f"Parsing line {line_number}, {dst = }, {src = }")
        t, v, s = decode_arg(dst)
        opcode_decoded["args"].append({'type': t, 'value': v, 'status': s})   # DST
        t, v, s = decode_arg(src)
        opcode_decoded["args"].append({'type': t, 'value': v, 'status': s})   # SRC
    else:
        raise Exception(f"Parsing error: Unknown opcode: {opcode} at line {line_number}")
    return opcode_decoded


def parse_data(line):
    data_decoded = {
        "type": "",
        "values": []
    }
    line_number = line[0]
    substrs = line[1].split(maxsplit=1)
    if len(substrs) < 2:
        raise Exception(f"Parsing error at line {line_number}: invalid data format: {line[1]}")
    # print(f"Parsing line {line_number}, {substrs = }, {len(substrs) = }")
    data_type, values = (substrs[0], substrs[1]) if len(substrs) == 2 else (substrs[0], substrs[1:])
    values = values.replace(",", " ")
    print(f"Parsing line {line_number}, {data_type = }, {values = }")
    if data_type == "DB":
        vals = values.split()
        data_decoded["type"] = "DB"
        for v in vals:
            if is_integer(v):
                data_decoded["values"].append(get_int_value(v))
    return data_decoded


def split_label_expression(text: str) -> list[str] | None:
    # Matches a valid Python identifier followed by contents inside square brackets
    match = re.match(r"^([a-zA-Z_][a-zA-Z0-9_]*)\[(.*)\]$", text)
    
    if match:
        return list(match.groups())
    return None


def is_label(text: str, include_colon=False, as_array=False) -> bool:
    # Matches a valid variable name starting with a letter or underscore, 
    # followed by letters, digits, or underscores, ending with a colon.
    if include_colon:
        pattern = r'^[a-zA-Z_][a-zA-Z0-9_]*:$'
    elif as_array:
        pattern = r'^[a-zA-Z_][a-zA-Z0-9_]*\[.*\]$'
    else:
        pattern = r'^[a-zA-Z_][a-zA-Z0-9_]*$'
    return bool(re.match(pattern, text))


def parse_expression(line_number, expression):
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
            if is_integer(element) or is_label(element):
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
        for _ in range(MAX_PASS_NUM):
            index = -1
            if isinstance(expr[0], list):
                _prepare_expression(expr[0])
            for i in range(1, len(expr), 2):
                if isinstance(expr[i+1], list):
                    _prepare_expression(expr[i])
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
    expression, _ = _parse_array(expression)
    expression = _prepare_expression(expression)
    parsed_expr = _parse_expression(expression)
    return parsed_expr


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
            if is_integer(element) or is_label(element):
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
                elif not (is_integer(expr[i]) or is_label(expr[i])):
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


def parse_constant(line):
    # Example: STACK_INIT EQU 0xFFFE, the right part can be either an integer, a label or an expression
    line_split = line[1].split(maxsplit=2)
    name = line_split[0]
    equ = line_split[1]
    if is_integer(line_split[2]):
        value = get_int_value(line_split[2])
        status = "resolved"
    elif is_label(line_split[2]):
        value = get_int_value(line_split[2])
        status = "resolved"
    elif is_expression(line[0], line_split[2]):
        value = parse_expression(line[0], line_split[2])
        status = "pending"
    else:
        raise Exception(f"Parsing error: Invalid expression on line {line[0]}: {line_split[2]}")
    return {"name": name, "value": value, "status": status}


def parse_lines(lines):
    constants = []
    labels = []
    current_label = None
    current_origin = 0
    binary = []
    for line in lines:
        line_split = line[1].split(maxsplit=2)
        if len(line_split) == 3 and line_split[1] == "EQU":
            value = parse_constant(line)
            constants.append(value)
        elif is_label(line[1], include_colon=True):
            label = line[1].replace(":", "")
            labels.append(label)
            current_label = label
        elif line[1].startswith("ORG"):
            current_origin = int(line[1].split()[1], 16)
            print(f"New origin: {current_origin}")
        elif line[1].split()[0] in opcodes:
            bin_code = parse_opcode(line)
            binary.append({"LN": line[0], "OP": line[1], "type": "OPCODE", "LABEL": current_label, "BIN": bin_code, "ADDR": current_origin})
            # current_origin += len(bin_code)//2
        elif line[1].split()[0] in ["DB"]:
            binary_data = parse_data(line)
            binary.append({"LN": line[0], "OP": line[1], "type": "DATA", "LABEL": current_label, "BIN": binary_data, "ADDR": current_origin})
    for c in constants:
        print(f"Constant {c}")
    # for label in labels:
    #     print(f"Label {label}")
    with open("parsed_code.json", 'w') as f:
        f.write("{\n")
        f.write("\"    constants\": [\n")
        lines = []
        for line in range(len(constants)):
            lines.append("        "+json.dumps(constants[line]))
        f.write(",\n".join(lines)+"\n    ],\n")
        f.write("\"    parsed_code\": [\n")
        lines = []
        for line in range(len(binary)):
            lines.append("        "+json.dumps(binary[line]))
        f.write(",\n".join(lines)+"\n    ]")
        f.write("}\n")
    # for line in range(10):
    #     print(f"Binary: {binary[line]}")
    # print(f"Binary: {binary[-1]}")



def main(filename="test.asm"):
    with open(filename) as f:
        lines = f.read().split("\n")
    clean_lines = []
    for line_num, line in enumerate(lines):
        temp = line.split(";")[0].strip()  # Remove comments
        if len(temp) > 0:
            clean_lines.append([line_num+1, temp])
    lines = clean_lines
    parse_lines(lines)
    # for i in range(10):
    #     print(lines[i])


if __name__ == "__main__":
    main()
