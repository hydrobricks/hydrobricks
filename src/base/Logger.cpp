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
