#include "cache.h"
#include <iostream>
#include <cassert>
int main(){
    // block=4 words, 16 slots, miss penalty 10
    Cache c(4, 16, 10);

    // First access to addr 0 -> MISS (cold)
    uint32_t l1 = c.access(0, false);
    assert(l1 == 10);  // miss penalty

    // addr 1,2,3 are in the SAME block as 0 (block=4 words) -> HITS
    assert(c.access(1,false)==1);
    assert(c.access(2,false)==1);
    assert(c.access(3,false)==1);

    // addr 4 is the next block -> MISS
    assert(c.access(4,false)==10);
    // re-access 4 -> HIT
    assert(c.access(4,false)==1);

    std::cout << "after sequential walk: hits="<<c.hits()
              <<" misses="<<c.misses()
              <<" hitRate="<<c.hitRate()<<"\n";

    // ---- conflict-miss demonstration ----
    // With 16 slots x 4 words/block, the cache spans 64 words. Two
    // addresses exactly 64 words apart map to the SAME slot -> they
    // evict each other on every alternating access.
    Cache c2(4, 16, 10);
    uint16_t A = 0, B = 64;   // both map to index 0
    for (int i=0;i<10;i++){ c2.access(A,false); c2.access(B,false); }
    std::cout << "conflict pattern: hits="<<c2.hits()
              <<" misses="<<c2.misses()
              <<" (expect almost all misses -> conflict thrashing)\n";

    // ---- locality demonstration: a hot loop variable ----
    Cache c3(4, 16, 10);
    for (int i=0;i<100;i++) c3.access(7,false);  // same addr 100x
    std::cout << "hot variable (100x same addr): hits="<<c3.hits()
              <<" misses="<<c3.misses()
              <<" hitRate="<<c3.hitRate()<<" AMAT="<<c3.amat()<<"\n";

    std::cout << "\nALL CACHE ASSERTIONS PASSED\n";
    std::cout << "config: " << c.config() << "\n";
    return 0;
}
