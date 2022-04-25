#include "Solver.h"

#include "Processor.h"

Solver::Solver()
    : m_processor(nullptr),
      m_nIterations(1)
{}

void Solver::InitializeContainers() {
    m_container.resize(m_processor->GetNbIterableValues(), m_nIterations);
}