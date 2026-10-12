

class Flags:
    def __init__(self):
        self.Z = 0
        self.C = 1
        self.N = 2

    def get_flag(self, flags, flag):
        return flags & (1<<flag) > 0

    def set_flag(self, flags, flag):
        return flags | (1<<flag)

    def clear_flag(self, flags, flag):
        return flags ^ (1<<flag)


class ALU:
    def __init__(self, flags):
        self.flags = flags
        self.reg0 = 0
        self.reg1 = 0
        self.result = 0
        self.opcode = 0

    def set_reg0(self, value):
        self.reg0 = value

    def set_reg1(self, value):
        self.reg1 = value

    def set_opcode(self, value):
        self.opcode = value

    def perform_operation(self):
        pass

    def get_result(self):
        return self.result
    
