#include "Connector.h"

#include "SubBasin.h"

Connector::Connector() {}

void Connector::Connect(SubBasin* in, SubBasin* out) {
    wxASSERT(in);
    wxASSERT(out);
    m_in = in;
    m_out = out;
    m_in->AddOutputConnector(this);
    m_out->AddInputConnector(this);
}
