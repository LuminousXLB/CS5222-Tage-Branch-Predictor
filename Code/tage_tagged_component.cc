#include "tage_tagged_component.h"
#include <cstdio>
#include <cstdlib>

#define saturate_update(val, inc, min, max)                                    \
    do {                                                                       \
        if (inc) {                                                             \
            if ((val) < (max)) {                                               \
                (val)++;                                                       \
            }                                                                  \
        } else {                                                               \
            if ((val) > (min)) {                                               \
                (val)--;                                                       \
            }                                                                  \
        }                                                                      \
    } while (0);

TaggedComponent::TaggedComponent(UInt32 bank_id,
                                 UInt32 tagged_loglen,
                                 UInt32 tagged_tag_width,
                                 UInt32 tagged_ctr_width,
                                 UInt32 tagged_hist_len,
                                 const TageGlobalHistory* history)
    : _id(bank_id)
    , _table_loglen(tagged_loglen)
    , _index_mask((1 << tagged_loglen) - 1)
    , _tag_mask((1 << tagged_tag_width) - 1)
    , _ctr_mask((1 << (tagged_ctr_width - 1)) - 1)
    , _table(1ULL << tagged_loglen)
    , _hist_len(tagged_hist_len)
    , _global(history)
    , _hist_idx(tagged_hist_len, tagged_loglen)
    , _hist_tag_0(tagged_hist_len, tagged_tag_width)
    , _hist_tag_1(tagged_hist_len, tagged_tag_width - 1) {

#ifdef DEBUG
    fprintf(stderr, "======================================\n");
    fprintf(stderr, "_table_loglen      = %u\n", _table_loglen);
    fprintf(stderr, "_index_mask        = %x\n", _index_mask);
    fprintf(stderr, "_tag_mask          = %x\n", _tag_mask);
    fprintf(stderr, "_ctr_mask          = %x\n", _ctr_mask);
    fprintf(stderr, "_table.size        = %lu\n", 1UL << tagged_loglen);
    fprintf(stderr, "_history.size      = %lu\n", _history->size());
    fprintf(stderr, "tagged_hist_len    = %u\n", tagged_hist_len);
    fprintf(stderr, "_hist_idx.clen     = %u\n", tagged_loglen);
    fprintf(stderr, "_hist_tag_0.clen   = %u\n", tagged_tag_width);
    fprintf(stderr, "_hist_tag_1.clen   = %u\n", tagged_tag_width - 1);
    fprintf(stderr, "======================================\n");
#endif
}

UInt32 TaggedComponent::gindex(IntPtr ip) const {
    /*
     * Reference:
     * - https://jilp.org/cbp/Pierre.tar.bz2.uue
     * - https://hpca23.cse.tamu.edu/taco/camino/cbp2/cbp-src/realistic-seznec.h
     */

#define ROTATE(x) (((x << _id) & _index_mask) + (x >> (_table_loglen - _id)))

    const auto hist_mask = (1 << std::min(_hist_len, 16u)) - 1;
    const auto hist      = _global->path & hist_mask;

    const auto histL = hist & _index_mask;
    const auto histH = hist >> _table_loglen;
    const auto mixed = histL ^ ROTATE(histH);

    const auto shift = std::abs(SInt64(_table_loglen) - _id) + 1;
    const auto index = ip ^ (ip >> shift) ^ _hist_idx.get() ^ ROTATE(mixed);

    return index & _index_mask;
#undef ROTATE
}

UInt16 TaggedComponent::gtag(IntPtr ip) const {
    /*
     * Reference:
     * - https://jilp.org/cbp/Pierre.tar.bz2.uue
     */

    UInt16 tag = ip ^ _hist_tag_0.get() ^ (_hist_tag_1.get() << 1);
    return tag & _tag_mask;
}

UInt8 TaggedComponent::use(IntPtr ip) const { return _table[gindex(ip)].use; }
UInt8& TaggedComponent::use(IntPtr ip) { return _table[gindex(ip)].use; }

SInt8 TaggedComponent::predict(IntPtr ip) const {
    const auto& entry = _table[gindex(ip)];
    if (entry.tag == gtag(ip)) {
        return entry.ctr >= 0 ? 1 : 0;
    } else {
        return -1;
    }
}

void TaggedComponent::allocate(bool actual, IntPtr ip) {
    auto& entry = _table[gindex(ip)];
    entry.tag   = gtag(ip);
    entry.ctr   = actual ? 0 : -1;
    entry.use   = 0;
}

void TaggedComponent::age(bool age_msb) {
    if (age_msb) {
        // clear least significant bits
        for (auto& entry : _table) {
            entry.use &= 2;
        }
    } else {
        // clear most significant bits
        for (auto& entry : _table) {
            entry.use &= 1;
        }
    }
}

void TaggedComponent::update_use(bool predict, bool actual, IntPtr ip) {
    constexpr int min = 0;
    constexpr int max = 3;
    saturate_update(_table[gindex(ip)].use, predict == actual, min, max);
}

void TaggedComponent::update_ctr(bool predict, bool actual, IntPtr ip) {
    const int min = -_ctr_mask;
    const int max = _ctr_mask - 1;
    saturate_update(_table[gindex(ip)].ctr, actual, min, max);
}

void TaggedComponent::update_history() {
    _hist_idx.update(_global->pred);
    _hist_tag_0.update(_global->pred);
    _hist_tag_1.update(_global->pred);
}
