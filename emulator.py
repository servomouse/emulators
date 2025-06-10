import threading
import time
import sys
from queue import Queue
from pynput import keyboard # pip install pynput

from brainfuck import BrainfuckPC
from logger import Logger


class UserInputGetter:
    def __init__(self):
        self.input_queue = Queue()
        self.part_input = ""    # Contains user input before they hit Enter
        self.command = None     # Contains user input after user hit Enter, become None after read
        self.event = threading.Event()  # Set when user hit Enter, can be used for smart wait
        self.listener = keyboard.Listener(on_press=self.key_press)
        self.listener.start()

    def get_part_input(self):
        return self.part_input

    def get_command(self):
        cmd = self.command
        self.command = None
        self.event.clear()
        return cmd

    def key_press(self, key):
        if key == keyboard.Key.enter:
            self.command = self.part_input
            self.part_input = ""
            self.event.set()
        elif key == keyboard.Key.backspace:
            self.part_input = self.part_input[:-1]
        elif key == keyboard.Key.space:
            self.part_input += " "
        elif hasattr(key, 'char'):
            self.part_input += key.char


def main():
    ui_getter = UserInputGetter()
    logger = Logger(ui_getter)
    pc = BrainfuckPC(binary="binaries/bf_hello_world.txt", logger=logger)
    pc.cpu.set_log_level(4)

    last_update = time.time()
    inner_state = 'stopped'
    while True:
        command = ui_getter.get_command()

        if command is not None:
            logger.print_line(f"User command from thread: {command}")
            if command == 'run':
                inner_state = 'running'
            elif command == 'print value':
                logger.print_line("Here is the value!")
            elif command == 'stop':
                inner_state = 'stopped'
            elif command == 'step':
                pc.clock.tick()
                inner_state = 'stopped'
            elif command == 'quit':
                pc.io.print_buffer_char()
                return

        if inner_state == 'running':
            if time.time()-last_update > 0.1:
                if 0 == pc.clock.tick():
                    inner_state = 'stopped'
                last_update = time.time()
        elif inner_state == 'stopped':
            logger.print_line("The thread is stopped")
            ui_getter.event.wait()


if __name__ == "__main__":
    main()
