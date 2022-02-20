#ifndef FOLDED_HISTORY_H
#define FOLDED_HISTORY_H

#include <cassert>
#include <cstdio>

#include "fixed_types.h"

/*
 * this is the cyclic shift register for folding a long global history into a
 * smaller number of bits
 *
 * Reference: https://jilp.org/cbp/Pierre.tar.bz2.uue
 */
class FoldedHistory {
    UInt32 _state;
    UInt32 _orig_length;
    UInt32 _comp_length;
    UInt32 _output_point;

  public:
    FoldedHistory(UInt32 original_length, UInt32 compressed_length)
        : _state(0)
        , _orig_length(original_length)
        , _comp_length(compressed_length)
        , _output_point(original_length % compressed_length) {}

    template <class Container> void update(const Container& history) {
        _state = (_state << 1) | history[0];
        _state ^= history[_orig_length] << _output_point;
        _state ^= (_state >> _comp_length);
        _state &= (1 << _comp_length) - 1;
    }

    UInt32 get() const { return _state; }
};

#endif
