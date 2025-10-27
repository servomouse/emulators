from brainfuck import BrainfuckPC
from logger import Logger


def main():

    logger = Logger(ui_getter=None)
    pc = BrainfuckPC(logger=logger)

    counter = 0
    while pc.clock.tick():
        counter += 1

    print(f"{counter = }")
    pc.io.print_buffer_char()


if __name__ == "__main__":
    main()
