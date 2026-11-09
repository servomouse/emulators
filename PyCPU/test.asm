; =====================================================================
; COMPREHENSIVE ISA TEST SUITE (SECTIONS, NEGATIVE VALUES, & '$' OPERATOR)
; Covers Opcodes: 0x00 through 0x1A
; =====================================================================

; --- CONSTANTS ---
STACK_INIT      EQU 0xFFFE
STACK_SIZE      EQU 0xFF
TEST_CONST      EQU STACK_INIT
IO_PORT_IN      EQU 0x10
IO_PORT_OUT     EQU 0x20
NEG_VALUE       EQU -23
EXPRESSION      EQU 2 + (STACK_INIT - IO_PORT_IN) + 4 * (2 / (IO_PORT_OUT+4))

; =====================================================================
; SECTION: DATA (Declared first in source, placed in data segment)
; =====================================================================
SECTION DATA, 0x0200    ; Directs assembler to place subsequent data at 0x0200

BYTE_ARR1:      UINT8 0x00 0x01 0x02 0x03
BYTE_ARR0:      UINT8 0x00, 0x01, 0x02, 0x03
VAR_COUNTER:    UINT8 0x40     ; 1-byte numeric counter variable
    UINT8 0x50
    UINT8 0x60
    UINT8 0x70
    UINT8 0x80
VAR_STATUS:     UINT8 0x01     ; 1-byte status flag variable
MSG_PTR:        UINT16 MSG_HELLO
EXPR_VAR:       UINT16 STACK_INIT - STACK_SIZE
WORD_VAR:       UINT16 0x0123
WORD_ARR:       UINT16 0x0123 0x2345 0x3456
MSG_HELLO:      CHAR "Hello World!", 0x00   ; Null-terminated ASCII string in RAM
ONE_MORE:
    UINT8 0x07

SECTION ENTRY, 0x0000
ENTRY: JMP START

SECTION TEXT, 0x0100    ; Directs assembler to place executable code at 0x0100

START:
    ; Initialize Stack Pointer and Address Register
    MOV SP, STACK_INIT
    
    ; Test reading/writing global RAM variables defined in DATA section
    MOV R0, 0x42
    MOV [VAR_COUNTER], R0   ; Store R0 into RAM variable COUNTER
    MOV R1, [VAR_COUNTER]   ; Load it back to verify RAM write/read

    MOV RA, 0x1234
    MOV R3, RA      ; R3: 0x34
    MOV R4, RA      ; R4: 0x12
    SWAP R3, R4     ; R3: 0x12, R4: 0x34
    MOV RA, R3      ; RA: 0x1212
    MOV RA, R4      ; RA: 0x3412

    MOV RA, MSG_HELLO+2
    MOV RA, MSG_HELLO       ; RA points to start of string
    MOV R2, [RA]            ; Load first character ('H') into R2
    MOV R2, (IO_PORT_IN+IO_PORT_OUT)/2

; =====================================================================
; 1. CONTROL & SYSTEM OPCODES (0x00, 0x18 - 0x1A)
; =====================================================================
TEST_SYSTEM:
    NOP                     ; (0x00) NOP
    IVT 0x0300              ; (0x18) IVT IMMED16 - Set Interrupt Vector Table offset
    EI 0x01                 ; (0x19) EI R/IMMED8 - Enable Interrupt 1
    DI 0x01                 ; (0x1A) DI R/IMMED8 - Disable Interrupt 1

; =====================================================================
; 2. DATA MOVEMENT OPCODES (0x01 - 0x03)
; =====================================================================
TEST_MOV_PUSH_POP:
    MOV R0, 0xAA            ; MOV R, IMMED8 (positive)
    MOV R1, -5              ; MOV R, IMMED8 (negative signed byte)
    MOV R2, R0              ; MOV R, R
    MOV [RA], R1            ; MOV (RA), R
    MOV [VAR_STATUS], R2    ; MOV (ADDR), R
    MOV R3, [RA]            ; MOV R, (RA)
    MOV R4, [VAR_STATUS]    ; MOV R, (ADDR)

    ; PUSH & POP variants with negative constants
    PUSH R0                 
    PUSH -128               ; PUSH negative IMMED8
    PUSH [RA]               
    PUSH [VAR_STATUS]

    POP R5                  
    POP [RA]                
    POP [VAR_STATUS]        

; =====================================================================
; 3. LOGICAL & BITWISE OPCODES (0x04 - 0x08)
; =====================================================================
TEST_LOGICAL:
    MOV R0, 0xF0
    MOV R1, 0x0F

    AND R0, R1              
    AND R0, 0xCC            
    OR R0, R1               
    OR R0, 0x11             
    XOR R0, R1              
    XOR R0, 0xFF            
    NOT R0                  
    NOT [RA]                

    SFT R0, 2               ; (0x08) Shift left by 2
    SFT R1, -2              ; (0x08) Shift right by 2 (negative signed count)

; =====================================================================
; 4. ARITHMETIC OPCODES (0x09 - 0x0D)
; =====================================================================
TEST_ARITHMETIC:
    MOV R0, 20
    MOV R1, 5

    ADD R0, R1              
    ADD R0, -3              ; ADD negative immediate
    SUB R0, R1              
    SUB R0, -5              ; SUB negative immediate (effectively adds 5)
    CMP R0, R1              
    CMP R0, 15              
    MUL R0, R1              
    MUL R0, -1              ; MUL negative immediate
    DIV R0, R1              
    DIV R0, 2               

; =====================================================================
; 5. CONTROL FLOW & JUMP OPCODES (0x0E - 0x12)
; =====================================================================
TEST_JUMPS:
    JMP REL_TARGET_1        
    NOP                     
REL_TARGET_1:

    JMP 0x0002              
    NOP                     
    JMP RA                
    JMP 0x0180            

    CMP R0, R0              ; Forces Zero flag (Z=1)
    JPZ REL_TARGET_2        
    NOP
REL_TARGET_2:

    ; Test backward relative jump using negative offset and '$' operator
    MOV R3, 3               ; Loop counter
LOOP_START:
    SUB R3, 1               
    CMP R3, 0               
    JNZ -4                  ; Jump backward relative by -4 bytes (to SUB R3, 1)

    ; Test self-referential jump using '$' (infinite loop in place)
    ; JMP $                   

    ; Test Negative flag jumps (JPG / JPL)
    MOV R7, 0x80            ; Set MSB to trigger N flag
    ADD R7, 0               
    JPG REL_TARGET_4        
    NOP
REL_TARGET_4:

    MOV R7, 0x01            ; Clear MSB (N=0)
    ADD R7, 0               
    JPL REL_TARGET_5        
    NOP
REL_TARGET_5:

; =====================================================================
; 6. SUBROUTINES, I/O & INTERRUPTS (0x13 - 0x17)
; =====================================================================
TEST_IO_AND_SUBS:
    IN R0, IO_PORT_IN       
    IN [RA], 0x11           
    OUT IO_PORT_OUT, R0     
    OUT 0x21, [RA]          

    INT 0x02                

    MOV RA, SUB_ROUTINE     
    CALL RA               
    JMP END_PROGRAM         

SUB_ROUTINE:
    MOV R5, 0xFF            
    RET                     

END_PROGRAM:
    NOP
    JMP $                   ; Infinite loop using current address operator ($)