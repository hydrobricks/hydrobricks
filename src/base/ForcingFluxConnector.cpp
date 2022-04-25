#include "ForcingFluxConnector.h"

ForcingFluxConnector::ForcingFluxConnector(Forcing* forcing, Flux* flux)
    : m_forcing(forcing),
      m_flux(flux)
{}
