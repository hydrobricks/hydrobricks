
#ifndef HYDROBRICKS_FLUX_H
#define HYDROBRICKS_FLUX_H

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

#endif  // HYDROBRICKS_FLUX_H
