#include "Connector.h"

#include "Subbasin.h"

Connector::Connector()
    : _in(nullptr),
      _out(nullptr) {}

void Connector::Connect(Subbasin* in, Subbasin* out) {
    assert(in);
    assert(out);
    _in = in;
    _out = out;
    _in->AddOutputConnector(this);
    _out->AddInputConnector(this);
}
