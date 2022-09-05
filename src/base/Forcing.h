#ifndef HYDROBRICKS_FORCING_H
#define HYDROBRICKS_FORCING_H

#include "Includes.h"
#include "TimeSeriesData.h"

class Forcing : public wxObject {
  public:
    explicit Forcing(VariableType type);

    ~Forcing() override = default;

    void AttachTimeSeriesData(TimeSeriesData* timeSeriesData);

    VariableType GetType() {
        return m_type;
    }

    double GetValue();

  protected:
    VariableType m_type;
    TimeSeriesData* m_timeSeriesData;

  private:
};

#endif  // HYDROBRICKS_FORCING_H
