#include "Solver.h"

#include <functional>
#include <unordered_map>
#include <vector>

#include "Processor.h"
#include "SolverAnalyticLinear.h"
#include "SolverEulerExplicit.h"
#include "SolverHeunExplicit.h"
#include "SolverRK4.h"

Solver::Solver()
    : _processor(nullptr) {}

static string GetValidSolverNames() {
    static const vector<string> validNames = {"rk4",
                                              "runge_kutta",  // Synonyms for RK4
                                              "euler_explicit",
                                              "heun_explicit",
                                              "analytic_linear",
                                              "analytic"};  // Synonyms

    string suggestions = "Valid solver names: ";
    for (size_t i = 0; i < validNames.size(); ++i) {
        suggestions += validNames[i];
        if (i < validNames.size() - 1) {
            suggestions += ", ";
        }
    }
    return suggestions;
}

std::unique_ptr<Solver> Solver::Factory(const SolverSettings& solverSettings) {
    using FactoryFunc = std::function<std::unique_ptr<Solver>()>;

    static const std::unordered_map<string, FactoryFunc> factoryMap = {
        {"rk4", []() { return std::make_unique<SolverRK4>(); }},
        {"runge_kutta", []() { return std::make_unique<SolverRK4>(); }},
        {"euler_explicit", []() { return std::make_unique<SolverEulerExplicit>(); }},
        {"heun_explicit", []() { return std::make_unique<SolverHeunExplicit>(); }},
        {"analytic_linear", []() { return std::make_unique<SolverAnalyticLinear>(); }},
        {"analytic", []() { return std::make_unique<SolverAnalyticLinear>(); }}};

    auto it = factoryMap.find(solverSettings.name);
    if (it != factoryMap.end()) {
        return it->second();
    }

    throw ModelConfigError(std::format("Incorrect solver name: {}. {}", solverSettings.name, GetValidSolverNames()));
}
