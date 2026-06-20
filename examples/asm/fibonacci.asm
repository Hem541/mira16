; ============================================================
;  fibonacci.asm
;  Compute Fibonacci numbers, storing each into memory.
;  Demonstrates: loops, register arithmetic, conditional exit.
;
;  R1 = previous fib   R2 = current fib   R3 = next = R1+R2
;  R4 = counter (how many left to compute)
;  R5 = memory write address
; ============================================================

        LOADI R1, 0          ; fib(0) = 0
        LOADI R2, 1          ; fib(1) = 1
        LOADI R4, 10         ; compute 10 numbers
        LOADI R5, 100        ; start storing at mem[100]
        LOADI R6, 1          ; constant 1, for decrementing R4

loop:   STORE R1, [100]      ; (note: simple demo store)
        ADD   R3, R1, R2     ; next = prev + curr
        MOV   R1, R2         ; prev = curr
        MOV   R2, R3         ; curr = next
        SUB   R4, R4, R6     ; counter -= 1
        JNZ   loop           ; if counter != 0, repeat

        HALT
