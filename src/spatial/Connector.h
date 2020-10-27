
#ifndef FLHY_CONNECTOR_H
#define FLHY_CONNECTOR_H

#include "Includes.h"

class SubBasin;

class Connector : public wxObject {
  public:
    Connector();

    ~Connector() override = default;

    void Connect(SubBasin* in, SubBasin* out);

  protected:
    SubBasin* m_in;
    SubBasin* m_out;

  private:
};

#endif  // FLHY_CONNECTOR_H
