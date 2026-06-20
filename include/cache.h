#ifndef CACHE_H
#define CACHE_H

#include <cstdint>
#include <vector>
#include <string>

// direct-mapped L1 cache. tracks hits/misses, reports amat.

class Cache {
public:
    // blockWords: words per block (power of 2). numSlots: cache slots
    // (power of 2). memLatency: cycles to reach main memory on a miss.
    Cache(uint32_t blockWords = 4, uint32_t numSlots = 16,
          uint32_t memLatency = 10);

    // Access one address. Returns the LATENCY in cycles for this access
    // (1 on a hit, memLatency on a miss). Records hit/miss stats. The
    // `isWrite` flag is for stats only; this is a simple write-through
    // model where the cache and memory both reflect the data.
    uint32_t access(uint16_t addr, bool isWrite);

    void reset();

    // ---- stats ------------------------------------------------------
    uint64_t hits()      const { return hits_; }
    uint64_t misses()    const { return misses_; }
    uint64_t accesses()  const { return hits_ + misses_; }
    double   hitRate()   const {
        uint64_t a = hits_ + misses_;
        return a ? (double)hits_ / (double)a : 0.0;
    }
    // Average memory access time (AMAT), in cycles: the headline metric.
    double   amat() const {
        uint64_t a = hits_ + misses_;
        if (!a) return 0.0;
        return (double)(hits_ * 1 + misses_ * memLatency_) / (double)a;
    }
    uint32_t memLatency() const { return memLatency_; }

    std::string config() const;

private:
    struct Line { bool valid = false; uint32_t tag = 0; };

    uint32_t blockWords_;   // words per block
    uint32_t numSlots_;     // number of cache slots
    uint32_t memLatency_;   // miss penalty in cycles

    uint32_t offsetBits_;   // log2(blockWords_)
    uint32_t indexBits_;    // log2(numSlots_)

    std::vector<Line> lines_;

    uint64_t hits_ = 0;
    uint64_t misses_ = 0;

    static uint32_t log2u(uint32_t x);
};

#endif // CACHE_H
