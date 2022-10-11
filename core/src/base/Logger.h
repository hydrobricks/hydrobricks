#ifndef HYDROBRICKS_LOGGER_H
#define HYDROBRICKS_LOGGER_H

#include "Includes.h"

class Logger : public wxObject {
  public:
    explicit Logger();

    ~Logger() override = default;

    void InitContainer(int timeSize, const vecInt& hydroUnitsIds, const vecStr& subBasinLabels,
                       const vecStr& hydroUnitLabels);

    void Reset();

    void SetSubBasinValuePointer(int iLabel, double* valPt);

    void SetHydroUnitValuePointer(int iUnit, int iLabel, double* valPt);

    void SetDate(double date);

    void Record();

    void Increment();

    bool DumpOutputs(const std::string& path);

    axd GetOutletDischarge();

    const vecAxd& GetSubBasinValues() {
        return m_subBasinValues;
    }

    const vecAxxd& GetHydroUnitValues() {
        return m_hydroUnitValues;
    }

  protected:
    int m_cursor;
    axd m_time;
    vecStr m_subBasinLabels;
    vecAxd m_subBasinValues;
    vecDoublePt m_subBasinValuesPt;
    vecInt m_hydroUnitIds;
    vecStr m_hydroUnitLabels;
    vecAxxd m_hydroUnitValues;
    std::vector<vecDoublePt> m_hydroUnitValuesPt;

  private:
};

#endif  // HYDROBRICKS_LOGGER_H
