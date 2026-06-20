; ============================================================
;  arraysum.asm  -  Sum values stored in memory, in a loop.
;  Designed to exercise the L1 cache: repeated access to a small
;  set of addresses shows high hit rate (temporal locality).
;
;  We store 5 into mem[200], then load-and-add it 15 times.
;  After the first cold miss, every memory access is a cache HIT.
;
;  R1 = value   R2 = accumulator   R3 = loaded value
;  R4 = loop counter   R6 = 1   R7 = 0
; ============================================================

        LOADI R1, 5
        LOADI R2, 0
        LOADI R4, 15
        LOADI R6, 1
        LOADI R7, 0
        STORE R1, [200]      ; one store (cold miss on this block)

loop:   LOAD  R3, [200]      ; repeated load of same addr -> cache hits
        ADD   R2, R2, R3     ; accumulate
        SUB   R4, R4, R6     ; counter--
        CMP   R4, R7
        JNZ   loop

        STORE R2, [201]      ; store final sum
        HALT
