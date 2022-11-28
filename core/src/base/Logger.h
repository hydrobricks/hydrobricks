#ifndef HYDROBRICKS_LOGGER_H
#define HYDROBRICKS_LOGGER_H

#include "Includes.h"
#include "SettingsModel.h"
#include "SubBasin.h"

class Logger : public wxObject {
  public:
    explicit Logger();

    ~Logger() override = default;

    void InitContainers(int timeSize, SubBasin* subBasin, SettingsModel& modelSettings);

    void Reset();

    void SetSubBasinValuePointer(int iLabel, double* valPt);

    void SetHydroUnitValuePointer(int iUnit, int iLabel, double* valPt);

    void SetHydroUnitFractionPointer(int iUnit, int iLabel, double* valPt);

    void SetDate(double date);

    void Record();

    void Increment();

    bool DumpOutputs(const string& path);

    axd GetOutletDischarge();

    vecInt GetIndicesForSubBasinElements(const string& item);

    vecInt GetIndicesForHydroUnitElements(const string& item);

    double GetTotalSubBasin(const string& item);

    double GetTotalHydroUnits(const string& item);

    double GetTotalOutletDischarge();

    double GetTotalET();

    double GetSubBasinInitialStorageState();

    double GetSubBasinFinalStorageState();

    double GetHydroUnitsInitialStorageState();

    double GetHydroUnitsFinalStorageState();

    double GetTotalStorageChanges();

    const vecAxd& GetSubBasinValues() {
        return m_subBasinValues;
    }

    const vecAxxd& GetHydroUnitValues() {
        return m_hydroUnitValues;
    }

  protected:
    int m_cursor;
    axd m_time;
    bool m_recordFractions;
    vecStr m_subBasinLabels;
    vecAxd m_subBasinValues;
    vecDoublePt m_subBasinValuesPt;
    vecInt m_hydroUnitIds;
    axd m_hydroUnitAreas;
    vecStr m_hydroUnitLabels;
    vecAxxd m_hydroUnitValues;
    std::vector<vecDoublePt> m_hydroUnitValuesPt;
    vecStr m_hydroUnitFractionLabels;
    vecAxxd m_hydroUnitFractions;
    std::vector<vecDoublePt> m_hydroUnitFractionsPt;

  private:
};

#endif  // HYDROBRICKS_LOGGER_H
