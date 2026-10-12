

class AddressSpace:
    def __init__(self, size):
        self.image = [0 for _ in range(size)]
        self.size = size

    def map_image(self, start_address, size, image):
        pass

    def map_device(self, addr_range, device):
        pass

    def read(self, addr):
        # Add mapped device processing
        return self.image[addr]

    def write(self, addr, value):
        # Add mapped device processing
        self.image[addr] = value
