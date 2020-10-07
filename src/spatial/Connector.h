
#ifndef FLHY_CONNECTOR_H
#define FLHY_CONNECTOR_H

#include "Includes.h"

class SubCatchment;

class Connector : public wxObject {
  public:
    Connector();

    ~Connector() override = default;

    void Connect(SubCatchment* in, SubCatchment* out);

  protected:
    SubCatchment* m_in;
    SubCatchment* m_out;

  private:
};

#endif  // FLHY_CONNECTOR_H
