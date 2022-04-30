#ifndef HYDROBRICKS_LOGGER_H
#define HYDROBRICKS_LOGGER_H

#include "Includes.h"

class Logger : public wxObject {
  public:
    explicit Logger();

    ~Logger() override = default;

    void InitContainer(int timeSize, int hydroUnitsNb, const vecStr &aggregatedLabels, const vecStr &hydroUnitLabels);

  protected:
    int m_cursor;
    axd m_time;
    vecStr m_aggregatedLabels;
    vecAxd m_aggregatedValues;
    vecInt m_hydroUnitIds;
    vecStr m_hydroUnitLabels;
    vecAxxd m_hydroUnitValues;

  private:
};

#endif  // HYDROBRICKS_LOGGER_H
