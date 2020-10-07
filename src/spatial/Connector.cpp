
#include "Connector.h"
#include "SubCatchment.h"

Connector::Connector() {}

void Connector::Connect(SubCatchment* in, SubCatchment* out) {
    wxASSERT(in);
    wxASSERT(out);
    m_in = in;
    m_out = out;
    m_in->AddOutputConnector(this);
    m_out->AddInputConnector(this);
}