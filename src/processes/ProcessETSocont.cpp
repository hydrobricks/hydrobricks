#include "ProcessETSocont.h"

ProcessETSocont::ProcessETSocont()
    : m_pet(0.0),
      m_stock(0.0),
      m_stockMax(0.0),
      m_exponent(0.5)
{}

double ProcessETSocont::GetWaterExtraction() {
    return m_pet * pow(m_stock / m_stockMax, m_exponent);
}
