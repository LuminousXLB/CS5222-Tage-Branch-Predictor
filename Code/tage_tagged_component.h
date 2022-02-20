#ifndef TAGE_TAGGED_COMPONENT_H
#define TAGE_TAGGED_COMPONENT_H

#include <deque>
#include <memory>
#include <vector>

#include "fixed_types.h"
#include "folded_history.h"

struct TageGlobalHistory {
    std::deque<UInt8> pred;
    UInt16 path;

    void update(bool actual, IntPtr ip) {
        /*
         * update prediction history
         */
        pred.pop_back();
        pred.push_front(actual);

        /*
         * update path history
         */
        path = (path << 1) | (ip & 1);
    }
};

class TaggedComponent {
    struct gentry {
        SInt8 ctr  = 0; // parameterized width
        UInt8 use  = 0; // 2-bit unsigned
        UInt16 tag = 0; // parameterized width
    };

  protected:
    const UInt32 _id;

    /*
     * Prediction Table
     */
    const UInt32 _table_loglen;
    const UInt32 _index_mask;
    const UInt32 _tag_mask;
    const UInt32 _ctr_mask;
    std::vector<gentry> _table; // prediction table

    /*
     * History Storage
     */
    const UInt32 _hist_len;           // length of prediction history
    const TageGlobalHistory* _global; // global history buffer

    FoldedHistory _hist_idx;   // index generation helper
    FoldedHistory _hist_tag_0; // tag generation helper
    FoldedHistory _hist_tag_1; // tag generation helper

    /*
     * Function calculating the index
     */
    UInt32 gindex(IntPtr ip) const;

    /*
     * Function calculating the tag
     */
    UInt16 gtag(IntPtr ip) const;

  public:
    TaggedComponent(UInt32 bank_id,
                    UInt32 tagged_loglen,
                    UInt32 tagged_tag_width,
                    UInt32 tagged_ctr_width,
                    UInt32 tagged_hist_len,
                    const TageGlobalHistory* history);

    UInt8 use(IntPtr ip) const;
    UInt8& use(IntPtr ip);

    SInt8 predict(IntPtr ip) const;

    void allocate(bool actual, IntPtr ip);
    void age(bool age_msb);

    void update_use(bool predict, bool actual, IntPtr ip);
    void update_ctr(bool predict, bool actual, IntPtr ip);
    void update_history();
};

#endif
