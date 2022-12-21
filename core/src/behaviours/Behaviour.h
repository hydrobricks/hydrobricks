#ifndef HYDROBRICKS_BEHAVIOUR_H
#define HYDROBRICKS_BEHAVIOUR_H

#include "BehavioursManager.h"
#include "Includes.h"

class Behaviour : public wxObject {
  public:
    Behaviour();

    ~Behaviour() override = default;

    bool virtual Apply(double date) = 0;

    void SetManager(BehavioursManager* manager) {
        m_manager = manager;
    }

    vecDouble GetDates() {
        return m_dates;
    }

  protected:
    BehavioursManager* m_manager;
    int m_cursor;
    vecDouble m_dates;

  private:
};

#endif  // HYDROBRICKS_BEHAVIOUR_H
