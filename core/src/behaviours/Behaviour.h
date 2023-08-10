#ifndef HYDROBRICKS_BEHAVIOUR_H
#define HYDROBRICKS_BEHAVIOUR_H

#include "BehavioursManager.h"
#include "Includes.h"

class Behaviour : public wxObject {
  public:
    Behaviour();

    ~Behaviour() override = default;

    void Reset();

    bool virtual Apply(double date);

    int GetIndexForInsertion(double date);

    void SetManager(BehavioursManager* manager) {
        m_manager = manager;
    }

    vecDouble GetDates() {
        return m_dates;
    }

    int GetItemsNb() {
        return (int)m_dates.size();
    }

    void IncrementCursor() {
        m_cursor++;
    }

  protected:
    BehavioursManager* m_manager;
    int m_cursor;
    vecDouble m_dates;

  private:
};

#endif  // HYDROBRICKS_BEHAVIOUR_H
