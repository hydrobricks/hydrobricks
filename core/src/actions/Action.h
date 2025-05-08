#ifndef HYDROBRICKS_ACTION_H
#define HYDROBRICKS_ACTION_H

#include "ActionsManager.h"
#include "Includes.h"

class Action : public wxObject {
  public:
    Action();

    ~Action() override = default;

    void Reset();

    bool virtual Apply(double date);

    int GetIndexForInsertion(double date);

    void SetManager(ActionsManager* manager) {
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
    ActionsManager* m_manager;
    int m_cursor;
    vecDouble m_dates;

  private:
};

#endif  // HYDROBRICKS_ACTION_H
