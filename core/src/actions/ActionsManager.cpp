#include "Action.h"
#include "ActionsManager.h"
#include "ModelHydro.h"
#include "Utils.h"

ActionsManager::ActionsManager()
    : m_model(nullptr),
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

    if (action->IsRecursive()) {
        m_recursiveActionIndices.push_back(actionIndex);
    } else {
        int index = 0;
        for (auto date : action->GetSporadicDates()) {
            // Reset the index if needed.
            if (!m_sporadicActionDates.empty() && m_sporadicActionDates[index] > date) {
                index = 0;
            }

            // Find index to insert the date accordingly.
            for (int i = index; i < m_sporadicActionDates.size(); ++i) {
                if (date <= m_sporadicActionDates[i]) {
                    break;
                }
                index++;
            }

            // Insert date and action.
            m_sporadicActionDates.insert(m_sporadicActionDates.begin() + index, date);
            m_sporadicActionIndices.insert(m_sporadicActionIndices.begin() + index, actionIndex);
        }
    }

    return true;
}

int ActionsManager::GetActionsNb() {
    return static_cast<int>(m_actions.size());
}

int ActionsManager::GetSporadicActionItemsNb() {
    int items = 0;
    for (auto action : m_actions) {
        items += action->GetSporadicItemsNb();
    }

    return items;
}

void ActionsManager::DateUpdate(double date) {
    // Recursive actions
    if (!m_recursiveActionIndices.empty()) {
        Time dateStruct = GetTimeStructFromMJD(date);
        for (int actionIndex : m_recursiveActionIndices) {
            if (!m_actions[actionIndex]->ApplyIfRecursive(dateStruct)) {
                throw InvalidArgument(_("Application of a recursive action failed."));
            }
        }
    }

    if (m_sporadicActionDates.empty()) {
        return;
    }

    // Sporadic actions
    wxASSERT(m_sporadicActionDates.size() == m_sporadicActionIndices.size());
    while (m_sporadicActionDates.size() > m_cursorManager && m_sporadicActionDates[m_cursorManager] <= date) {
        if (!m_actions[m_sporadicActionIndices[m_cursorManager]]->Apply(date)) {
            throw InvalidArgument(_("Application of a sporadic action failed."));
        }
        m_actions[m_sporadicActionIndices[m_cursorManager]]->IncrementCursor();
        m_cursorManager++;
    }
}

HydroUnit* ActionsManager::GetHydroUnitById(int id) {
    return m_model->GetSubBasin()->GetHydroUnitById(id);
}
