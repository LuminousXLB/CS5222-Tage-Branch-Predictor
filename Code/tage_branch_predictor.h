#ifndef TAGE_BRANCH_PREDICTOR_H
#define TAGE_BRANCH_PREDICTOR_H

#include <deque>
#include <vector>

#include "branch_predictor.h"
#include "simple_bimodal_table.h"
#include "tage_tagged_component.h"

struct TageConfig {
    SInt32 tagged_count        = 8;
    UInt32 tagged_loglen       = 12;
    UInt32 tagged_tag_width    = 11;
    UInt32 tagged_ctr_width    = 3;
    UInt32 tagged_min_hist_len = 4;
    UInt32 tagged_max_hist_len = 640;
};

class TageBranchPredictor : public BranchPredictor {

  private:
    /*
     * Bimodal Predictor Parameters
     */
    SimpleBimodalTable _p_base;

    /*
     * Tagged Components Parameters
     */
    std::vector<TaggedComponent> _p_tagged; // tagged components

    /*
     * Global History Buffer
     */
    TageGlobalHistory _global;

    /*
     * States presisted across predict and update
     */
    struct {
        SInt32 hit_bank;
        SInt32 alt_bank;
    } _bridge;

    /*
     * Tick count
     */
    UInt32 _tick;

    /*
     * Function to calculate predict result
     */
    bool _predict(SInt32 bank_id, IntPtr ip);

  public:
    TageBranchPredictor(String name,
                        core_id_t core_id,
                        TageConfig config,
                        UInt32 bimodal_loglen = 14);

    bool predict(bool indirect, IntPtr ip, IntPtr target);
    void update(
        bool predicted, bool actual, bool indirect, IntPtr ip, IntPtr target);
};

#endif
