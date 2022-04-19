#ifndef HYDROBRICKS_HYDRO_UNIT_H
#define HYDROBRICKS_HYDRO_UNIT_H

#include "Brick.h"
#include "HydroUnitProperty.h"
#include "Includes.h"

class HydroUnit : public wxObject {
  public:
    enum Types {
        Distributed,
        SemiDistributed,
        Lumped,
        Undefined
    };

    HydroUnit(float area = UNDEFINED, Types type = Undefined);

    ~HydroUnit() override;

    void AddProperty(HydroUnitProperty* property);

    void AddContainer(Brick* container);

    void AddFlux(Flux* flux);

    Types GetType() {
        return m_type;
    }

    void SetId(long id) {
        m_id = id;
    }

    float GetArea() {
        return m_area;
    }

    long GetId() const {
        return m_id;
    }

  protected:
    Types m_type;
    long m_id;
    float m_area; // m2
    std::vector<HydroUnitProperty*> m_properties;
    std::vector<Brick*> m_containers;
    std::vector<Flux*> m_fluxes;

  private:
};

#endif
