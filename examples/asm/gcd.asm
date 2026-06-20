; ============================================================
;  gcd.asm  -  Greatest Common Divisor via the Euclidean algorithm,
;  implemented as a SUBROUTINE called with CALL / RET.
;
;  Demonstrates the crown jewels: subroutine calls, the stack as a
;  return-address mechanism, DIV, and a real loop with a remainder.
;
;  gcd(a, b): while b != 0 { t = a mod b; a = b; b = t }  -> a
;  We compute gcd(48, 36) which is 12.
;
;  Convention (a tiny "calling convention" you can explain!):
;    R1 = a   (input, and final result)
;    R2 = b   (input)
;    R3 = quotient scratch   R4 = product scratch   R5 = remainder
;    R7 = constant 0
; ============================================================

        LOADI R1, 48         ; a
        LOADI R2, 36         ; b
        LOADI R7, 0          ; constant zero

        CALL  gcd            ; <-- pushes return address, jumps to gcd
        HALT                 ; when gcd RETs, we land here. R1 holds answer.

; ---- subroutine: gcd ---------------------------------------
gcd:    CMP   R2, R7         ; is b == 0 ?
        JZ    gcd_done       ; if so, a is the answer -> return
        DIV   R3, R1, R2     ; R3 = a / b   (integer quotient)
        MUL   R4, R3, R2     ; R4 = quotient * b
        SUB   R5, R1, R4     ; R5 = a - (quotient*b) = a mod b  (remainder)
        MOV   R1, R2         ; a = b
        MOV   R2, R5         ; b = remainder
        JMP   gcd            ; loop
gcd_done:
        RET                  ; <-- pops return address back into PC
