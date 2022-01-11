; MSP430 variable delay routine
;   void delay_cycles_runtime(unsigned short cycles);
;
; Works like __delay_cycles, but the delay needn't be a compile-time constant
; Requires zero wait state instruction fetch; run from RAM as necessary
; Delay is clamped to 18 cycles minimum. Total size is 28 bytes

.text
    .def delay_cycles_runtime

softbp: .macro
        .word 0x4343 ; NOP used by CCS for software breakpoints
        .endm

delay_cycles_runtime:
        sub #19, R12 ; Subtract overhead: call/ret, setup, one loop (9c+7c+3c)
        jhs entry    ; Skip first loop's NOP to save 1 cycle of overhead
        jmp delay3   ; Requested delay is below overhead, so delay 18 cycles

        ; 4 cycle delay loop
loop:   nop
entry:  sub #4, R12
        jhs loop

        ; Handle remainder
        rla R12      ; Double R12 so it can be used as a PC offset
        sub R12, PC  ; Offset is negative, so subtracting it jumps forward
        softbp       ; Offset  0: never hit, but required for code layout
delay3: nop          ; Offset -2: 3 cycles remainder
        nop          ; Offset -4: 2 cycles remainder
        nop          ; Offset -6: 1 cycle remainder
        ret          ; Offset -8: 0 cycles remainder
