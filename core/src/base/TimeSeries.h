#ifndef HYDROBRICKS_TIME_SERIES_H
#define HYDROBRICKS_TIME_SERIES_H

#include "Includes.h"
#include "TimeSeriesData.h"

class TimeSeries : public wxObject {
  public:
    explicit TimeSeries(VariableType type);

    ~TimeSeries() override = default;

    static bool Parse(const wxString &path, std::vector<TimeSeries*> &vecTimeSeries);

    virtual bool SetCursorToDate(double date) = 0;

    virtual bool AdvanceOneTimeStep() = 0;

    virtual bool IsDistributed() = 0;

    virtual double GetStart() = 0;

    virtual double GetEnd() = 0;

    virtual TimeSeriesData* GetDataPointer(int unitId) = 0;

    VariableType GetVariableType() {
        return m_type;
    }

  protected:
    VariableType m_type;

  private:
};

#endif  // HYDROBRICKS_TIME_SERIES_H
