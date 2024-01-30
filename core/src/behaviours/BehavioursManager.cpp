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

void BehavioursManager::Reset() {
    m_cursorManager = 0;
    for (auto behaviour : m_behaviours) {
        behaviour->Reset();
    }
}

bool BehavioursManager::AddBehaviour(Behaviour* behaviour) {
    wxASSERT(behaviour);
    behaviour->SetManager(this);

    int behaviourIndex = int(m_behaviours.size());
    m_behaviours.push_back(behaviour);

    int index = 0;
    for (auto date : behaviour->GetDates()) {
        // Reset the index if needed.
        if (!m_dates.empty() && m_dates[index] > date) {
            index = 0;
        }

        // Find index to insert the date accordingly.
        for (int i = index; i < m_dates.size(); ++i) {
            if (date <= m_dates[i]) {
                break;
            }
            index++;
        }

        // Insert date and behaviour.
        m_dates.insert(m_dates.begin() + index, date);
        m_behaviourIndices.insert(m_behaviourIndices.begin() + index, behaviourIndex);
    }
    m_active = true;

    return true;
}

int BehavioursManager::GetBehavioursNb() {
    return (int)m_behaviours.size();
}

int BehavioursManager::GetBehaviourItemsNb() {
    int items = 0;
    for (auto behaviour : m_behaviours) {
        items += behaviour->GetItemsNb();
    }

    return items;
}

void BehavioursManager::DateUpdate(double date) {
    if (!m_active) {
        return;
    }
    wxASSERT(m_dates.size() == m_behaviourIndices.size());

    while (m_dates.size() > m_cursorManager && m_dates[m_cursorManager] <= date) {
        if (!m_behaviours[m_behaviourIndices[m_cursorManager]]->Apply(date)) {
            throw InvalidArgument(_("Application of a behaviour failed."));
        }
        m_behaviours[m_behaviourIndices[m_cursorManager]]->IncrementCursor();
        m_cursorManager++;
    }
}

HydroUnit* BehavioursManager::GetHydroUnitById(int id) {
    return m_model->GetSubBasin()->GetHydroUnitById(id);
}
