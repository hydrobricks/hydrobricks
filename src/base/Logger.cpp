#include "Logger.h"

Logger::Logger()
    : m_cursor(0)
{}

void Logger::InitContainer(int timeSize, int hydroUnitsNb, const vecStr &aggregatedLabels, const vecStr &hydroUnitLabels) {
    m_time.resize(timeSize);
    m_aggregatedLabels = aggregatedLabels;
    m_aggregatedValues = vecAxd(aggregatedLabels.size(), axd::Zero(timeSize));
    m_aggregatedValuesPt.resize(aggregatedLabels.size());
    m_hydroUnitIds.resize(hydroUnitsNb);
    m_hydroUnitLabels = hydroUnitLabels;
    m_hydroUnitValues = vecAxxd(hydroUnitLabels.size(), axxd::Zero(timeSize, hydroUnitsNb));
    m_hydroUnitValuesPt = std::vector<vecDoublePt>(hydroUnitLabels.size(), vecDoublePt(hydroUnitsNb, nullptr));
}

void Logger::SetAggregatedValuePointer(int iLabel, double* valPt) {
    wxASSERT(m_aggregatedValuesPt.size() > iLabel);
    m_aggregatedValuesPt[iLabel] = valPt;
}

void Logger::SetHydroUnitValuePointer(int iUnit, int iLabel, double* valPt) {
    wxASSERT(m_hydroUnitValuesPt.size() > iLabel);
    wxASSERT(m_hydroUnitValuesPt[iLabel].size() > iUnit);
    m_hydroUnitValuesPt[iLabel][iUnit] = valPt;
}

void Logger::SetDateTime(double dateTime) {
    wxASSERT(m_cursor < m_time.size());
    m_time[m_cursor] = dateTime;
}

void Logger::Record() {
    wxASSERT(m_cursor < m_time.size());

    for (int iAggregated = 0; iAggregated < m_aggregatedValuesPt.size(); ++iAggregated) {
        wxASSERT(m_aggregatedValuesPt[iAggregated]);
        m_aggregatedValues[iAggregated][m_cursor] = *m_aggregatedValuesPt[iAggregated];
    }

    for (int iUnitVal = 0; iUnitVal < m_hydroUnitValuesPt.size(); ++iUnitVal) {
        for (int iUnit = 0; iUnit < m_hydroUnitValues[iUnitVal].cols(); ++iUnit) {
            wxASSERT(m_hydroUnitValuesPt[iUnitVal][iUnit]);
            m_hydroUnitValues[iUnitVal](m_cursor, iUnit) = *m_hydroUnitValuesPt[iUnitVal][iUnit];
        }
    }
}

void Logger::Increment() {
    m_cursor++;
}
