#include "ActionsManager.h"

#include "Action.h"
#include "ModelHydro.h"
#include "SubBasin.h"
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
        action->ResetCursor();
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

int ActionsManager::GetActionCount() const {
    return static_cast<int>(_actions.size());
}

int ActionsManager::GetSporadicActionItemCount() const {
    int items = 0;
    for (auto& action : _actions) {
        items += action->GetSporadicItemCount();
    }
    return items;
}

void ActionsManager::DateUpdate(double date) {
    // Recursive actions
    if (!_recursiveActionIndices.empty()) {
        Time dateStruct = GetTimeStructFromMJD(date);
        for (int actionIndex : _recursiveActionIndices) {
            if (!_actions[actionIndex]->ApplyIfRecursive(dateStruct)) {
                throw RuntimeError(_("Application of a recursive action failed."));
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
            throw RuntimeError(_("Application of a sporadic action failed."));
        }
        _actions[_sporadicActionIndices[_cursorManager]]->IncrementCursor();
        _cursorManager++;
    }
}

SubBasin* ActionsManager::GetSubBasin() const {
    return _model->GetSubBasin();
}

HydroUnit* ActionsManager::GetHydroUnitById(int id) const {
    return _model->GetSubBasin()->GetHydroUnitById(id);
}

bool ActionsManager::IsValid() const {
    // Check that model is assigned
    if (!_model) {
        wxLogError(_("ActionsManager: Model not assigned."));
        return false;
    }

    return true;
}

void ActionsManager::Validate() const {
    if (!IsValid()) {
        throw ModelConfigError(_("ActionsManager validation failed. Model not properly assigned."));
    }
}
