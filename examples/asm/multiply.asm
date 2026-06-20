; ============================================================
;  multiply.asm
;  Compute R3 = R1 * R2 by repeated addition.
;  Demonstrates: stack (PUSH/POP), CMP, conditional looping.
;
;  We multiply 6 * 7 = 42.
;  R1 = multiplicand (added repeatedly)
;  R2 = counter (how many times left to add)
;  R3 = running product (accumulator)
;  R6 = constant 1
;  R7 = constant 0 (for CMP against R2)
; ============================================================

        LOADI R1, 6          ; multiplicand
        LOADI R2, 7          ; multiplier / loop count
        LOADI R3, 0          ; product accumulator
        LOADI R6, 1          ; decrement constant
        LOADI R7, 0          ; zero, to test the counter

        PUSH  R2             ; demo: save the original multiplier on stack

loop:   CMP   R2, R7         ; counter == 0 ?
        JZ    done           ; if yes, finished
        ADD   R3, R3, R1     ; product += multiplicand
        SUB   R2, R2, R6     ; counter -= 1
        JMP   loop

done:   POP   R4             ; demo: restore original multiplier into R4
        HALT
