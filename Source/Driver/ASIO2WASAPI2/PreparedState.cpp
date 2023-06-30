//
// Created by whyask37 on 2023-06-30.
//

#include "PreparedState.h"

static const uint64_t twoRaisedTo32 = UINT64_C(4294967296);

ASIOError PreparedState::getSamplePosition(ASIOSamples *sPos, ASIOTimeStamp *tStamp) const {
    if (tStamp) {
        tStamp->lo = m_theSystemTime.lo;
        tStamp->hi = m_theSystemTime.hi;
    }
    if (sPos) {
        if (m_samplePosition >= twoRaisedTo32) {
            sPos->hi = (unsigned long) (m_samplePosition / twoRaisedTo32);
            sPos->lo = (unsigned long) (m_samplePosition - (sPos->hi * twoRaisedTo32));
        } else {
            sPos->hi = 0;
            sPos->lo = (unsigned long) m_samplePosition;
        }
    }
    return ASE_OK;
}