#include "Logger.h"

Logger::Logger()
    : m_cursor(0)
{}

void Logger::InitContainer(int timeSize, int hydroUnitsNb, const vecStr &subBasinLabels, const vecStr &hydroUnitLabels) {
    m_time.resize(timeSize);
    m_subBasinLabels = subBasinLabels;
    m_subBasinValues = vecAxd(subBasinLabels.size(), axd::Zero(timeSize));
    m_subBasinValuesPt.resize(subBasinLabels.size());
    m_hydroUnitIds.resize(hydroUnitsNb);
    m_hydroUnitLabels = hydroUnitLabels;
    m_hydroUnitValues = vecAxxd(hydroUnitLabels.size(), axxd::Zero(timeSize, hydroUnitsNb));
    m_hydroUnitValuesPt = std::vector<vecDoublePt>(hydroUnitLabels.size(), vecDoublePt(hydroUnitsNb, nullptr));
}

void Logger::SetSubBasinValuePointer(int iLabel, double* valPt) {
    wxASSERT(m_subBasinValuesPt.size() > iLabel);
    m_subBasinValuesPt[iLabel] = valPt;
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

    for (int iSubBasin = 0; iSubBasin < m_subBasinValuesPt.size(); ++iSubBasin) {
        wxASSERT(m_subBasinValuesPt[iSubBasin]);
        m_subBasinValues[iSubBasin][m_cursor] = *m_subBasinValuesPt[iSubBasin];
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
