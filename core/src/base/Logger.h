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

    void SaveInitialValues();

    void Record();

    void Increment();

    bool DumpOutputs(const string& path);

    axd GetOutletDischarge();

    vecInt GetIndicesForSubBasinElements(const string& item);

    vecInt GetIndicesForHydroUnitElements(const string& item);

    double GetTotalSubBasin(const string& item);

    double GetTotalHydroUnits(const string& item, bool needsAreaWeighting = false);

    double GetTotalOutletDischarge();

    double GetTotalET();

    double GetSubBasinInitialStorageState(const string& tag);

    double GetSubBasinFinalStorageState(const string& tag);

    double GetHydroUnitsInitialStorageState(const string& tag);

    double GetHydroUnitsFinalStorageState(const string& tag);

    double GetTotalWaterStorageChanges();

    double GetTotalSnowStorageChanges();

    double GetTotalGlacierStorageChanges();

    const vecAxd& GetSubBasinValues() {
        return m_subBasinValues;
    }

    const vecAxxd& GetHydroUnitValues() {
        return m_hydroUnitValues;
    }

    void RecordFractions() {
        m_recordFractions = true;
    }

  protected:
    int m_cursor;
    axd m_time;
    bool m_recordFractions;
    vecStr m_subBasinLabels;
    axd m_subBasinInitialValues;
    vecAxd m_subBasinValues;
    vecDoublePt m_subBasinValuesPt;
    vecInt m_hydroUnitIds;
    axd m_hydroUnitAreas;
    vecStr m_hydroUnitLabels;
    vecAxd m_hydroUnitInitialValues;
    vecAxxd m_hydroUnitValues;
    vector<vecDoublePt> m_hydroUnitValuesPt;
    vecStr m_hydroUnitFractionLabels;
    vecAxxd m_hydroUnitFractions;
    vector<vecDoublePt> m_hydroUnitFractionsPt;

  private:
};

#endif  // HYDROBRICKS_LOGGER_H
