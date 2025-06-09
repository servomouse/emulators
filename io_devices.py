

class IODevice:
    def __init__(self, input_callback=None):
        self.output_buffer = []
        self.input_callback = input_callback
    
    def io_output(self, addr, value):
        print(f"IO_Device: output byte: {value}")
        self.output_buffer.append(value)
    
    def io_input(self, addr):
        ret_val = 0
        if self.input_callback is not None:
            ret_val = self.input_callback
        print(f"IO_Device: input byte: {ret_val}")
        return ret_val
    
    def print_buffer(self):
        print(f"IO_Device buffer: {self.output_buffer}")
    
    def print_buffer_char(self):
        output = "".join([chr(i) for i in self.output_buffer])
        print(f"IO_Device buffer chars: {output}")
