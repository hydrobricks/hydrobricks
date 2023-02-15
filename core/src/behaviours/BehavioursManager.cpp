#include "BehavioursManager.h"

#include "Behaviour.h"
#include "ModelHydro.h"

BehavioursManager::BehavioursManager()
    : m_active(false),
      m_model(nullptr),
      m_cursorManager(0) {}

void BehavioursManager::SetModel(ModelHydro* model) {
    m_model = model;
}

bool BehavioursManager::AddBehaviour(Behaviour* behaviour) {
    wxASSERT(behaviour);
    behaviour->SetManager(this);

    int behaviourIndex = int(m_behaviours.size());
    m_behaviours.push_back(behaviour);

    if (m_dates.empty()) {
        m_dates = behaviour->GetDates();
        m_behaviourIndices = vecInt(m_dates.size(), behaviourIndex);
    } else {
        int index = 0;
        for (auto date : behaviour->GetDates()) {
            for (int i = index; i < m_dates.size(); ++i) {
                if (m_dates[i] <= date) {
                    break;
                }
                index++;
            }
            m_dates.insert(m_dates.begin() + index, date);
            m_behaviourIndices.insert(m_behaviourIndices.begin() + index, behaviourIndex);
        }
    }
    m_active = true;

    return true;
}

int BehavioursManager::GetBehavioursNb() {
    return (int)m_behaviours.size();
}

void BehavioursManager::DateUpdate(double date) {
    if (!m_active) {
        return;
    }
    wxASSERT(m_dates.size() == m_behaviourIndices.size());

    while (m_dates.size() > m_cursorManager && m_dates[m_cursorManager] <= date) {
        if (!m_behaviours[m_behaviourIndices[m_cursorManager]]->Apply(date)) {
            wxLogError(_("Application of a behaviour failed."));
        }
        m_behaviours[m_behaviourIndices[m_cursorManager]]->IncrementCursor();
        m_cursorManager++;
    }
}

HydroUnit* BehavioursManager::GetHydroUnitById(int id) {
    return m_model->GetSubBasin()->GetHydroUnitById(id);
}