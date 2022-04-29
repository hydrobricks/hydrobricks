#include "ProcessETSocont.h"

ProcessETSocont::ProcessETSocont(Brick* brick)
    : Process(brick),
      m_pet(nullptr),
      m_stock(nullptr),
      m_stockMax(0.0),
      m_exponent(0.5)
{}

double ProcessETSocont::GetWaterExtraction() {
    return m_pet->GetValue() * pow(*m_stock / m_stockMax, m_exponent);
}
