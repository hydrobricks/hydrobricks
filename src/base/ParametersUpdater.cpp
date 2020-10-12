
#include "ParametersUpdater.h"

ParametersUpdater::ParametersUpdater() {}

void ParametersUpdater::DateUpdate(float date) {

}

void ParametersUpdater::ChangingYear(int year) {
    for (auto & parameter : m_parametersYearly) {
        parameter->UpdateParameter(year);
    }
}

void ParametersUpdater::ChangingMonth(wxDateTime::Month month) {
    for (auto & parameter : m_parametersMonthly) {
        parameter->UpdateParameter(month);
    }
}

void ParametersUpdater::ChangingDate(float date) {
    for (auto & parameter : m_parametersDates) {
        parameter->UpdateParameter(date);
    }
}