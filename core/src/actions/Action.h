#ifndef HYDROBRICKS_ACTION_H
#define HYDROBRICKS_ACTION_H

#include "ActionsManager.h"
#include "Includes.h"

class Action : public wxObject {
  public:
    Action();

    ~Action() override = default;

    void Reset();

    bool virtual Apply();

    bool virtual Apply(double date);

    bool virtual ApplyIfRecursive(const Time date);

    int GetIndexForInsertion(double date);

    void SetManager(ActionsManager* manager) {
        m_manager = manager;
    }

    vecDouble GetSporadicDates() {
        return m_sporadicDates;
    }

    int GetSporadicItemsNb() {
        return (int)m_sporadicDates.size();
    }

    void IncrementCursor() {
        m_cursor++;
    }

    bool IsRecursive() {
        return m_recursive;
    }

  protected:
    ActionsManager* m_manager;
    int m_cursor;
    vecDouble m_sporadicDates;
    bool m_recursive;
    vecInt m_recursiveMonths;
    vecInt m_recursiveDays;

  private:
};

#endif  // HYDROBRICKS_ACTION_H
