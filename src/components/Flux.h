
#ifndef FLHY_FLUX_H
#define FLHY_FLUX_H

#include "Includes.h"

class Container;
class Modifier;

class Flux : public wxObject {
  public:
    explicit Flux();

    ~Flux() override = default;

  protected:
    Container* m_in;
    Container* m_out;
    Modifier* m_modifier;

  private:
};

#endif  // FLHY_FLUX_H
