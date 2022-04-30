#include "Logger.h"

Logger::Logger()
    : m_cursor(0)
{}

void Logger::InitContainer(int timeSize, int hydroUnitsNb, const vecStr &aggregatedLabels, const vecStr &hydroUnitLabels) {
    m_time.resize(timeSize);
    m_aggregatedLabels = aggregatedLabels;
    m_aggregatedValues = vecAxd(aggregatedLabels.size(), axd::Zero(timeSize));
    m_hydroUnitIds.resize(hydroUnitsNb);
    m_hydroUnitLabels = hydroUnitLabels;
    m_hydroUnitValues = vecAxxd(aggregatedLabels.size(), axxd::Zero(timeSize, hydroUnitsNb));
}
