#include "ActionsManager.h"

#include "Action.h"
#include "ModelHydro.h"
#include "Utils.h"

ActionsManager::ActionsManager()
    : _model(nullptr),
      _cursorManager(0) {}

void ActionsManager::SetModel(ModelHydro* model) {
    _model = model;
}

void ActionsManager::Reset() {
    _cursorManager = 0;
    for (auto action : _actions) {
        action->Reset();
    }
}

bool ActionsManager::AddAction(Action* action) {
    wxASSERT(action);
    action->SetManager(this);
    if (!action->Init()) {
        wxLogError(_("Action initialization failed."));
        return false;
    }

    int actionIndex = static_cast<int>(_actions.size());
    _actions.push_back(action);

    if (action->IsRecursive()) {
        _recursiveActionIndices.push_back(actionIndex);
    } else {
        int index = 0;
        for (auto date : action->GetSporadicDates()) {
            // Reset the index if needed.
            if (!_sporadicActionDates.empty() && _sporadicActionDates[index] > date) {
                index = 0;
            }

            // Find index to insert the date accordingly.
            for (int i = index; i < _sporadicActionDates.size(); ++i) {
                if (date <= _sporadicActionDates[i]) {
                    break;
                }
                index++;
            }

            // Insert date and action.
            _sporadicActionDates.insert(_sporadicActionDates.begin() + index, date);
            _sporadicActionIndices.insert(_sporadicActionIndices.begin() + index, actionIndex);
        }
    }

    return true;
}

int ActionsManager::GetActionsNb() {
    return static_cast<int>(_actions.size());
}

int ActionsManager::GetSporadicActionItemsNb() {
    int items = 0;
    for (auto action : _actions) {
        items += action->GetSporadicItemsNb();
    }

    return items;
}

void ActionsManager::DateUpdate(double date) {
    // Recursive actions
    if (!_recursiveActionIndices.empty()) {
        Time dateStruct = GetTimeStructFromMJD(date);
        for (int actionIndex : _recursiveActionIndices) {
            if (!_actions[actionIndex]->ApplyIfRecursive(dateStruct)) {
                throw InvalidArgument(_("Application of a recursive action failed."));
            }
        }
    }

    if (_sporadicActionDates.empty()) {
        return;
    }

    // Sporadic actions
    wxASSERT(_sporadicActionDates.size() == _sporadicActionIndices.size());
    while (_sporadicActionDates.size() > _cursorManager && _sporadicActionDates[_cursorManager] <= date) {
        if (!_actions[_sporadicActionIndices[_cursorManager]]->Apply(date)) {
            throw InvalidArgument(_("Application of a sporadic action failed."));
        }
        _actions[_sporadicActionIndices[_cursorManager]]->IncrementCursor();
        _cursorManager++;
    }
}

HydroUnit* ActionsManager::GetHydroUnitById(int id) {
    return _model->GetSubBasin()->GetHydroUnitById(id);
}