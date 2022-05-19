#include <gtest/gtest.h>

#include "ModelHydro.h"
#include "SettingsModel.h"
#include "SettingsBasin.h"
#include "TimeSeriesUniform.h"

/**
 * Model: simple snowpack model
 */

class FluxWeightedModel : public ::testing::Test {
  protected:
    SettingsModel m_model;
    TimeSeriesUniform* m_tsPrecip{};

    virtual void SetUp() {
        m_model.SetSolver("HeunExplicit");
        m_model.SetTimer("2020-01-01", "2020-01-10", 1, "Day");

        // Surface elements
        m_model.AddSurfaceBrick("item-1", "GenericSurface");
        m_model.AddSurfaceBrick("item-2", "GenericSurface");
        m_model.GenerateSurfaceComponents();

        // Direct outflow processes
        m_model.SelectBrick("item-1");
        m_model.AddProcessToCurrentBrick("outflow", "Outflow:direct");
        m_model.AddLoggingToCurrentProcess("output");
        m_model.AddOutputToCurrentProcess("surface-1");
        m_model.SelectBrick("item-2");
        m_model.AddProcessToCurrentBrick("outflow", "Outflow:direct");
        m_model.AddLoggingToCurrentProcess("output");
        m_model.AddOutputToCurrentProcess("surface-2");

        // Surface bricks
        m_model.SelectBrick("surface-1");
        m_model.AddProcessToCurrentBrick("outflow", "Outflow:direct");
        m_model.AddLoggingToCurrentProcess("output");
        m_model.AddOutputToCurrentProcess("outlet", true);
        m_model.SelectBrick("surface-2");
        m_model.AddProcessToCurrentBrick("outflow", "Outflow:direct");
        m_model.AddLoggingToCurrentProcess("output");
        m_model.AddOutputToCurrentProcess("outlet", true);

        m_model.AddLoggingToItem("outlet");

        auto precip = new TimeSeriesDataRegular(wxDateTime(1, wxDateTime::Jan, 2020),
                                                wxDateTime(10, wxDateTime::Jan, 2020), 1, Day);
        precip->SetValues({0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0, 0.0});
        m_tsPrecip = new TimeSeriesUniform(Precipitation);
        m_tsPrecip->SetData(precip);
    }
    virtual void TearDown() {
        wxDELETE(m_tsPrecip);
    }
};

TEST_F(FluxWeightedModel, SingleUnitWith1Brick100Percent) {
    SettingsBasin basinProp;
    basinProp.AddHydroUnit(1, 100);
    basinProp.AddSurfaceElementToCurrentUnit("item-1", 1.0);
    basinProp.AddSurfaceElementToCurrentUnit("item-2", 0.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinProp));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(m_model));
    EXPECT_TRUE(model.IsOk());

    EXPECT_TRUE(subBasin.AssignRatios(basinProp));

    ASSERT_TRUE(model.AddTimeSeries(m_tsPrecip));
    ASSERT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());
/*
    // Check resulting discharge
    vecAxd basinOutputs = model.GetLogger()->GetAggregatedValues();

    vecDouble expectedOutputs = {0.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 0.0, 0.0};

    for (auto & basinOutput : basinOutputs) {
        for (int j = 0; j < basinOutput.size(); ++j) {
            EXPECT_NEAR(basinOutput[j], expectedOutputs[j], 0.000001);
        }
    }
    /*
    // Check melt and swe
    vecAxxd unitContent = model.GetLogger()->GetHydroUnitValues();

    vecDouble expectedSWE = {0.0, 10.0, 20.0, 25.0, 25.0, 22.0, 16.0, 7.0, 0.0, 0.0};
    vecDouble expectedMelt = {0.0, 0.0, 0.0, 0.0, 0.0, 3.0, 6.0, 9.0, 7.0, 0.0};

    for (int j = 0; j < expectedSWE.size(); ++j) {
        EXPECT_NEAR(unitContent[0](j, 0), expectedSWE[j], 0.000001);
        EXPECT_NEAR(unitContent[1](j, 0), expectedMelt[j], 0.000001);
    }*/
}
