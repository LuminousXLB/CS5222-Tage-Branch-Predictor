#include "branch_predictor.h"
#include "a53branchpredictor.h"
#include "config.hpp"
#include "one_bit_branch_predictor.h"
#include "pentium_m_branch_predictor.h"
#include "simulator.h"
#include "stats.h"
#include "tage_branch_predictor.h"

BranchPredictor::BranchPredictor()
    : m_correct_predictions(0)
    , m_incorrect_predictions(0) {}

BranchPredictor::BranchPredictor(String name, core_id_t core_id)
    : m_correct_predictions(0)
    , m_incorrect_predictions(0) {
    registerStatsMetric(name, core_id, "num-correct", &m_correct_predictions);
    registerStatsMetric(name,
                        core_id,
                        "num-incorrect",
                        &m_incorrect_predictions);
}

BranchPredictor::~BranchPredictor() {}

UInt64 BranchPredictor::m_mispredict_penalty;

#define CFG_INT(key)                                                           \
    cfg->getIntArray("perf_model/branch_predictor/" key, core_id)

BranchPredictor* BranchPredictor::create(core_id_t core_id) {
    try {
        config::Config* cfg = Sim()->getCfg();
        assert(cfg);

        m_mispredict_penalty =
            cfg->getIntArray("perf_model/branch_predictor/mispredict_penalty",
                             core_id);

        String type =
            cfg->getStringArray("perf_model/branch_predictor/type", core_id);

        if (type == "none") {
            return 0;
        } else if (type == "one_bit") {
            UInt32 size =
                cfg->getIntArray("perf_model/branch_predictor/size", core_id);
            return new OneBitBranchPredictor("branch_predictor", core_id, size);
        } else if (type == "pentium_m") {
            return new PentiumMBranchPredictor("branch_predictor", core_id);
        } else if (type == "a53") {
            return new A53BranchPredictor("branch_predictor", core_id);
        } else if (type == "tage") {

            UInt32 bimodal_loglen = CFG_INT("bimodal_loglen");

            TageConfig config;
            config.tagged_count        = CFG_INT("tagged_count");
            config.tagged_loglen       = CFG_INT("tagged_loglen");
            config.tagged_tag_width    = CFG_INT("tagged_tag_width");
            config.tagged_ctr_width    = CFG_INT("tagged_ctr_width");
            config.tagged_min_hist_len = CFG_INT("tagged_min_hist_len");
            config.tagged_max_hist_len = CFG_INT("tagged_max_hist_len");

            return new TageBranchPredictor("branch_predictor",
                                           core_id,
                                           config,
                                           bimodal_loglen);
        } else {
            LOG_PRINT_ERROR("Invalid branch predictor type.");
            return 0;
        }
    } catch (...) {
        LOG_PRINT_ERROR(
            "Config info not available while constructing branch predictor.");
        return 0;
    }
}

#undef CFG_INT

UInt64 BranchPredictor::getMispredictPenalty() { return m_mispredict_penalty; }

void BranchPredictor::resetCounters() {
    m_correct_predictions   = 0;
    m_incorrect_predictions = 0;
}

void BranchPredictor::updateCounters(bool predicted, bool actual) {
    if (predicted == actual)
        ++m_correct_predictions;
    else
        ++m_incorrect_predictions;
}
