#include "cache.h"
#include <sstream>

// log2 of a power-of-two value (counts trailing zeros essentially).
uint32_t Cache::log2u(uint32_t x) {
    uint32_t n = 0;
    while (x > 1) { x >>= 1; ++n; }
    return n;
}

Cache::Cache(uint32_t blockWords, uint32_t numSlots, uint32_t memLatency)
    : blockWords_(blockWords), numSlots_(numSlots), memLatency_(memLatency) {
    offsetBits_ = log2u(blockWords_);
    indexBits_  = log2u(numSlots_);
    lines_.assign(numSlots_, Line{});
}

void Cache::reset() {
    for (auto& l : lines_) { l.valid = false; l.tag = 0; }
    hits_ = misses_ = 0;
}

// The core: decompose the address, check the mapped slot, hit or miss.
uint32_t Cache::access(uint16_t addr, bool /*isWrite*/) {
    // ---- split the address: [ tag | index | offset ] ----
    uint32_t blockAddr = addr >> offsetBits_;          // drop the offset
    uint32_t index     = blockAddr & (numSlots_ - 1);  // low indexBits_ bits
    uint32_t tag       = blockAddr >> indexBits_;      // the rest

    Line& line = lines_[index];

    // ---- HIT: slot valid and tag matches ----
    if (line.valid && line.tag == tag) {
        ++hits_;
        return 1;               // fast: 1 cycle
    }

    // ---- MISS: fetch block from memory, install it, record tag ----
    ++misses_;
    line.valid = true;
    line.tag = tag;             // (the actual data lives in real memory;
                                //  the cache only tracks which block is here)
    return memLatency_;         // slow: pay the full penalty
}

std::string Cache::config() const {
    std::ostringstream o;
    o << "direct-mapped, " << numSlots_ << " slots x " << blockWords_
      << " words/block (" << (numSlots_*blockWords_) << " words total), "
      << "miss penalty " << memLatency_ << " cyc";
    return o.str();
}
