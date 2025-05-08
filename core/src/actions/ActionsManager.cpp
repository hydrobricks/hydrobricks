#include "Action.h"
#include "ActionsManager.h"
#include "ModelHydro.h"

ActionsManager::ActionsManager()
    : m_active(false),
      m_model(nullptr),
      m_cursorManager(0) {}

void ActionsManager::SetModel(ModelHydro* model) {
    m_model = model;
}

void ActionsManager::Reset() {
    m_cursorManager = 0;
    for (auto action : m_actions) {
        action->Reset();
    }
}

bool ActionsManager::AddAction(Action* action) {
    wxASSERT(action);
    action->SetManager(this);

    int actionIndex = static_cast<int>(m_actions.size());
    m_actions.push_back(action);

    int index = 0;
    for (auto date : action->GetDates()) {
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

        // Insert date and action.
        m_dates.insert(m_dates.begin() + index, date);
        m_actionIndices.insert(m_actionIndices.begin() + index, actionIndex);
    }
    m_active = true;

    return true;
}

int ActionsManager::GetActionsNb() {
    return static_cast<int>(m_actions.size());
}

int ActionsManager::GetActionItemsNb() {
    int items = 0;
    for (auto action : m_actions) {
        items += action->GetItemsNb();
    }

    return items;
}

void ActionsManager::DateUpdate(double date) {
    if (!m_active) {
        return;
    }
    wxASSERT(m_dates.size() == m_actionIndices.size());

    while (m_dates.size() > m_cursorManager && m_dates[m_cursorManager] <= date) {
        if (!m_actions[m_actionIndices[m_cursorManager]]->Apply(date)) {
            throw InvalidArgument(_("Application of a action failed."));
        }
        m_actions[m_actionIndices[m_cursorManager]]->IncrementCursor();
        m_cursorManager++;
    }
}

HydroUnit* ActionsManager::GetHydroUnitById(int id) {
    return m_model->GetSubBasin()->GetHydroUnitById(id);
}
