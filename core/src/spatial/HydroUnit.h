#ifndef HYDROBRICKS_HYDRO_UNIT_H
#define HYDROBRICKS_HYDRO_UNIT_H

#include "Brick.h"
#include "Forcing.h"
#include "HydroUnitProperty.h"
#include "Includes.h"
#include "LandCover.h"
#include "Splitter.h"

class HydroUnit : public wxObject {
  public:
    enum Types {
        Distributed,
        SemiDistributed,
        Lumped,
        Undefined
    };

    HydroUnit(double area = UNDEFINED, Types type = Undefined);

    ~HydroUnit() override;

    void Reset();

    void SaveAsInitialState();

    void AddProperty(HydroUnitProperty* property);

    void AddBrick(Brick* brick);

    void AddSplitter(Splitter* splitter);

    bool HasForcing(VariableType type);

    void AddForcing(Forcing* forcing);

    Forcing* GetForcing(VariableType type);

    int GetBricksCount();

    int GetSplittersCount();

    Brick* GetBrick(int index);

    bool HasBrick(const string& name);

    Brick* GetBrick(const string& name);

    LandCover* GetLandCover(const string& name);

    Splitter* GetSplitter(int index);

    bool HasSplitter(const string& name);

    Splitter* GetSplitter(const string& name);

    bool IsOk();

    bool ChangeLandCoverAreaFraction(const string& name, double fraction);

    bool FixLandCoverFractionsTotal();

    Types GetType() {
        return m_type;
    }

    void SetId(int id) {
        m_id = id;
    }

    double GetArea() const {
        return m_area;
    }

    int GetId() const {
        return m_id;
    }

  protected:
    Types m_type;
    int m_id;
    double m_area;  // m2
    vector<HydroUnitProperty*> m_properties;
    vector<Brick*> m_bricks;
    vector<LandCover*> m_landCoverBricks;
    vector<Splitter*> m_splitters;
    vector<Forcing*> m_forcing;

  private:
};

#endif
