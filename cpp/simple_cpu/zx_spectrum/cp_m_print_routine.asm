; --- Address 0xF000: CP/M BDOS Entry Point ---
ORG 0xF000

BDOS_ENTRY:
    PUSH AF             ; Save registers to avoid corrupting test state
    PUSH BC
    PUSH DE
    PUSH HL

    LD A, C             ; Load function code into A
    CP 02h              ; Function 2: Character output?
    JR Z, PRINT_CHAR
    CP 09h              ; Function 9: String output?
    JR Z, PRINT_STRING
    JR EXIT_BDOS        ; Otherwise, just return

PRINT_CHAR:
    LD A, E             ; The character to print is in E
    OUT (01h), A        ; Output to port 0x01 (or whichever port you prefer)
    JR EXIT_BDOS

PRINT_STRING:
    ; DE points to the start of the string
    EX DE, HL           ; Move string pointer to HL for easier access
STRING_LOOP:
    LD A, (HL)          ; Load character
    CP '$'              ; Check for CP/M string terminator
    JR Z, EXIT_BDOS
    OUT (01h), A        ; Output character to the host
    INC HL              ; Next character
    JR STRING_LOOP

EXIT_BDOS:
    POP HL              ; Restore registers
    POP DE
    POP BC
    POP AF
    RET                 ; Return to the calling program (ZEXALL)

; Compiled code:
; F5 C5 D5 E5 79 FE 02 28 06 FE 09 28 07 18 10 7B
; D3 01 18 0B EB 7E FE 24 28 05 D3 01 23 18 F6 E1
; D1 C1 F1 C9