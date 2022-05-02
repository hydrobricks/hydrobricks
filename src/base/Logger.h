#ifndef HYDROBRICKS_LOGGER_H
#define HYDROBRICKS_LOGGER_H

#include "Includes.h"

class Logger : public wxObject {
  public:
    explicit Logger();

    ~Logger() override = default;

    void InitContainer(int timeSize, int hydroUnitsNb, const vecStr &aggregatedLabels, const vecStr &hydroUnitLabels);

    void SetAggregatedValuePointer(int iLabel, double* valPt);

    void SetHydroUnitValuePointer(int iUnit, int iLabel, double* valPt);

    void SetDateTime(double date);

    void Record();

    void Increment();

    const vecAxd& GetAggregatedValues() {
        return m_aggregatedValues;
    }

    const vecAxxd& GetHydroUnitValues() {
        return m_hydroUnitValues;
    }

  protected:
    int m_cursor;
    axd m_time;
    vecStr m_aggregatedLabels;
    vecAxd m_aggregatedValues;
    vecDoublePt m_aggregatedValuesPt;
    vecInt m_hydroUnitIds;
    vecStr m_hydroUnitLabels;
    vecAxxd m_hydroUnitValues;
    std::vector<vecDoublePt> m_hydroUnitValuesPt;

  private:
};

#endif  // HYDROBRICKS_LOGGER_H
