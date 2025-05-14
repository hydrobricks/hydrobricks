#ifndef HYDROBRICKS_MODIFIER_LAG_H
#define HYDROBRICKS_MODIFIER_LAG_H

#include "Includes.h"
#include "Modifier.h"

class ModifierLag : public Modifier {
  public:
    explicit ModifierLag();

    ~ModifierLag() override = default;
};

#endif  // HYDROBRICKS_MODIFIER_LAG_H
