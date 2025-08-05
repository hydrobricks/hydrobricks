#ifndef HYDROBRICKS_MODIFIER_H
#define HYDROBRICKS_MODIFIER_H

#include "Includes.h"

class Modifier : public wxObject {
  public:
    explicit Modifier();

    ~Modifier() override = default;
};

#endif  // HYDROBRICKS_MODIFIER_H
