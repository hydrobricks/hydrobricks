#ifndef HYDROBRICKS_BEHAVIOUR_UPDATER_H
#define HYDROBRICKS_BEHAVIOUR_UPDATER_H

#include "Includes.h"

class ModelHydro;
class Behaviour;

class BehavioursManager : public wxObject {
  public:
    BehavioursManager();

    ~BehavioursManager() override = default;

    ModelHydro* GetModel() {
        return m_model;
    }

    void SetModel(ModelHydro* model);

    bool AddBehaviour(Behaviour* behaviour);

    void DateUpdate(double date);

  protected:
    bool m_active;
    ModelHydro* m_model;
    int m_cursor;
    std::vector<Behaviour*> m_behaviours;
    vecDouble m_dates;
    vecInt m_behaviourIndices;

  private:
};

#endif  // HYDROBRICKS_BEHAVIOUR_UPDATER_H
