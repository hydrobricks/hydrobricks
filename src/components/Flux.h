#ifndef HYDROBRICKS_FLUX_H
#define HYDROBRICKS_FLUX_H

#include "Includes.h"

class Brick;
class Modifier;

class Flux : public wxObject {
  public:
    explicit Flux();

    ~Flux() override = default;

  protected:
    Brick* m_in;
    Brick* m_out;
    Modifier* m_modifier;

  private:
};

#endif  // HYDROBRICKS_FLUX_H
