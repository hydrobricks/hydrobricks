#include "Connector.h"

#include "SubBasin.h"

Connector::Connector()
    : _in(nullptr),
      _out(nullptr) {}

void Connector::Connect(SubBasin* in, SubBasin* out) {
    wxASSERT(in);
    wxASSERT(out);
    _in = in;
    _out = out;
    _in->AddOutputConnector(this);
    _out->AddInputConnector(this);
}
