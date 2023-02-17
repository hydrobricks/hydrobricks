#ifndef HYDROBRICKS_BEHAVIOUR_UPDATER_H
#define HYDROBRICKS_BEHAVIOUR_UPDATER_H

#include "HydroUnit.h"
#include "Includes.h"

class ModelHydro;
class Behaviour;

class BehavioursManager : public wxObject {
  public:
    BehavioursManager();

    ~BehavioursManager() override = default;

    void Reset();

    ModelHydro* GetModel() {
        return m_model;
    }

    void SetModel(ModelHydro* model);

    bool AddBehaviour(Behaviour* behaviour);

    int GetBehavioursNb();

    int GetBehaviourItemsNb();

    void DateUpdate(double date);

    HydroUnit* GetHydroUnitById(int id);

    vecDouble GetDates() {
        return m_dates;
    }

  protected:
    bool m_active;
    ModelHydro* m_model;
    int m_cursorManager;
    std::vector<Behaviour*> m_behaviours;
    vecDouble m_dates;
    vecInt m_behaviourIndices;

  private:
};

#endif  // HYDROBRICKS_BEHAVIOUR_UPDATER_H
