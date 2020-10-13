
#ifndef FLHY_CONTAINER_H
#define FLHY_CONTAINER_H

#include "Includes.h"

class Container : public wxObject {
  public:
    Container();

    ~Container() override = default;

    double GetWaterContent() {
        return m_waterContent;
    }

  protected:
    double m_waterContent;

  private:
};

#endif  // FLHY_CONTAINER_H
