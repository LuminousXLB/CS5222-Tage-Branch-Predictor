#include "tage_branch_predictor.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>

TageBranchPredictor::TageBranchPredictor(String name,
                                         core_id_t core_id,
                                         TageConfig config,
                                         UInt32 bimodal_loglen)
    : BranchPredictor(name, core_id)
    , _p_base(bimodal_loglen)
    , _tick(0) {

    /*
     * Initialize the global history buffer
     */
    _global.pred.resize(config.tagged_max_hist_len + 1, 0);
    _global.path = 0;

    /*
     * Calculate the q of the geometric series
     *
     * l_1 = min
     * l_n = max = min * q^{n-1}
     * log(max) - log(min) = (n-1) log q
     * q = exp( (log(max) - log(min)) / (n-1) )
     */
    const double q = exp(
        (log(config.tagged_max_hist_len) - log(config.tagged_min_hist_len)) /
        (config.tagged_count - 1));

    /*
     * Create the tagged components
     */
    auto tagged_hist_len = config.tagged_min_hist_len;
    for (SInt32 bank_id = 0; bank_id < config.tagged_count; bank_id++) {
        tagged_hist_len =
            std::min<UInt32>(config.tagged_max_hist_len,
                             bank_id == 0 ? config.tagged_min_hist_len
                                          : tagged_hist_len * q + 0.5);

        _p_tagged.emplace_back(bank_id,
                               config.tagged_loglen,
                               config.tagged_tag_width,
                               config.tagged_ctr_width,
                               tagged_hist_len,
                               &_global);
    }
}

bool TageBranchPredictor::_predict(SInt32 bank_id, IntPtr ip) {
    if (bank_id >= 0) {
        return _p_tagged[bank_id].predict(ip);
    } else {
        return _p_base.predict(false, ip, 0);
    }
}

bool TageBranchPredictor::predict(bool indirect, IntPtr ip, IntPtr target) {
    SInt32 alt_bank = -1;
    SInt32 hit_bank = -1;

    /*
     * find the last & alternative components with valid entry
     */
    const SInt32 _t_count = _p_tagged.size();

    for (SInt32 bank_id = 0; bank_id < _t_count; bank_id++) {
        auto p = _p_tagged[bank_id].predict(ip);
        if (p >= 0) {
            alt_bank = hit_bank;
            hit_bank = bank_id;
        }
    }

    /*
     * save the information across predict and update
     */
    _bridge = {.hit_bank = hit_bank, .alt_bank = alt_bank};

    /*
     * produce prediction result
     */
    return _predict(hit_bank, ip);
}

void TageBranchPredictor::update(
    bool predicted, bool actual, bool indirect, IntPtr ip, IntPtr target) {
    /*
     * ensure the result is logged
     */
    updateCounters(predicted, actual);

    /*
     * periodically age useful counter
     */
    _tick++;
    constexpr UInt32 tick_bits = 19;
    constexpr UInt32 tick_mask = (1 << tick_bits) - 1;

    if ((_tick & tick_mask) == 0) {
        for (auto& tagged_component : _p_tagged) {
            tagged_component.age((_tick >> (tick_bits - 1)) & 1);
        }
    }

    /*
     * update useful counter of the hit bank
     */
    const SInt32 hit_bank = _bridge.hit_bank;
    const SInt32 alt_bank = _bridge.alt_bank;
    const bool hit_pred   = _predict(hit_bank, ip);
    const bool alt_pred   = _predict(alt_bank, ip);
    assert(hit_pred == predicted);
    if (hit_bank > -1 && hit_pred != alt_pred) {
        _p_tagged[hit_bank].update_use(hit_pred, actual, ip);
    }

    /*
     * allocate a new entry
     */
    const SInt32 _t_count = _p_tagged.size();
    if (predicted != actual && hit_bank + 1 < _t_count) {
        /*
         * find available slots
         */
        std::vector<SInt32> candidate;
        candidate.reserve(_t_count - alt_bank);
        for (SInt32 bank_id = hit_bank + 1; bank_id < _t_count; bank_id++) {
            if (_p_tagged[bank_id].use(ip) == 0) {
                candidate.push_back(bank_id);
            }
        }

        /*
         * take longer history length by a prob of 3/4
         */
        if (!candidate.empty()) {
            /*
             * take a slot that utilize longer history and allocate a new entry
             */
            const UInt32 slot_idx =
                std::min<UInt32>(rand() & 3, candidate.size() - 1);

            _p_tagged[candidate[slot_idx]].allocate(actual, ip);
        } else {
            /*
             * or fade others
             */
            fprintf(stderr, "all fade invoked\n");
            for (SInt32 bank_id = hit_bank + 1; bank_id < _t_count; bank_id++) {
                _p_tagged[bank_id].use(ip)--;
            }
        }
    }

    /*
     * update prediction counter
     */
    if (hit_bank > -1) {
        _p_tagged[hit_bank].update_ctr(predicted, actual, ip);
    }

    if (alt_bank > -1 && _p_tagged[hit_bank].use(ip) == 0) {
        _p_tagged[alt_bank].update_ctr(predicted, actual, ip);
    }

    /*
     * update base bimodal prediction table
     */
    _p_base.update(predicted, actual, indirect, ip, target);

    /*
     * update global history
     */
    _global.update(actual, ip);
    for (auto& tagged_component : _p_tagged) {
        tagged_component.update_history();
    }
}
