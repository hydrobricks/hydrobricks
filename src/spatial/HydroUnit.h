
#ifndef FLHY_HYDRO_UNIT_H
#define FLHY_HYDRO_UNIT_H

#include "Includes.h"
#include "Container.h"
#include "HydroUnitProperty.h"

class HydroUnit : public wxObject {
  public:
    enum Types {
        Distributed,
        SemiDistributed,
        Lumped,
        Undefined
    };

    HydroUnit(float area = UNDEFINED, Types type = Undefined);

    ~HydroUnit() override = default;

    void AddProperty(HydroUnitProperty* property);

    void AddContainer(Container* container);

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
    std::vector<Container*> m_containers;
    std::vector<Flux*> m_fluxes;

  private:
};

#endif
