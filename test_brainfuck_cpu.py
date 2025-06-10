from brainfuck import BrainfuckPC


def main():

    pc = BrainfuckPC()

    counter = 0
    while pc.clock.tick():
        counter += 1

    print(f"{counter = }")
    pc.io.print_buffer_char()


if __name__ == "__main__":
    main()
