

class Clock:
    def __init__(self, frequency):
        self.frequency = frequency
        self.devices = []
        self.counter = 0
    
    def tick(self):
        """ Returns True for success and False in case of error """
        for dev in self.devices:
            if 0 != dev.module_tick(self.counter):
                return False
        self.counter += 1
        return True
    
    def add_device(self, device):
        if not device in self.devices:
            self.devices.append(device)
    
    def remove_device(self, device):
        if device in self.devices:
            self.devices.remove(device)
