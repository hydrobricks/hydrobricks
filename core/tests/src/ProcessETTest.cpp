#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "ModelHydro.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"

/**
 * Tests for the actual evapotranspiration (PET -> AET) reduction processes:
 * et:linear, et:power_law and et:exponential.
 *
 * All three live on a soil storage brick and reduce PET as a function of the
 * storage filling ratio. The tests use a minimal but identical wiring so that the
 * only difference between two runs is the ET process itself; this lets us check the
 * limiting-case identities exactly (power_law(0.5) == socont, power_law(1.0) == linear).
 */
namespace {

/// Build a minimal valid model: rain -> soil storage (capacity 100) with the given ET
/// process, draining linearly to the outlet. paramName/paramValue set the ET parameter
/// when the process has one (empty paramName for parameter-free processes).
void BuildSoilModel(SettingsModel& model, const std::string& etKind, const std::string& paramName = "",
                    float paramValue = 0.0f) {
    model.SetSolver("heun_explicit");
    model.SetTimer("2020-01-01", "2020-01-10", 1, "day");
    model.SetLogAll(true);

    // Precipitation (rain only, no snow)
    model.GeneratePrecipitationSplitters(false);

    // Land cover routing the incoming rain straight to the soil store
    model.AddLandCoverBrick("ground", "generic_land_cover");
    model.SelectHydroUnitBrick("ground");
    model.AddBrickProcess("throughfall", "outflow:direct", "soil");
    model.SetProcessOutputsAsInstantaneous();

    // Soil storage with the ET process under test
    model.AddHydroUnitBrick("soil", "storage");
    model.AddBrickParameter("capacity", 100.0f);
    model.AddBrickProcess("et", etKind);
    if (!paramName.empty()) {
        model.SetProcessParameterValue(paramName, paramValue);
    }
    model.AddBrickProcess("outflow", "outflow:linear", "outlet");
    model.SetProcessParameterValue("response_factor", 0.2f);
    model.AddBrickProcess("overflow", "overflow", "outlet");

    model.AddLoggingToItem("outlet");
}

std::unique_ptr<TimeSeriesUniform> MakeForcing(VariableType type, const vecDouble& values) {
    auto data = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 10), 1, TimeUnit::Day);
    data->SetValues(values);
    auto ts = std::make_unique<TimeSeriesUniform>(type);
    ts->SetData(std::move(data));
    return ts;
}

/// Run the given model on a fixed precipitation/PET forcing and return its logger results.
struct RunResults {
    double precip;
    double discharge;
    double et;
    double storage;
};

RunResults RunSoilModel(SettingsModel& settings) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);
    basinSettings.AddLandCover("ground", "", 1.0);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(settings, basinSettings));
    EXPECT_TRUE(model.IsValid());

    auto precip = MakeForcing(VariableType::Precipitation, {0.0, 20.0, 20.0, 20.0, 20.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    auto pet = MakeForcing(VariableType::PET, {2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0});

    EXPECT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(precip))));
    EXPECT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(pet))));
    EXPECT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();
    return {80.0, logger->GetTotalOutletDischarge(), logger->GetTotalET(), logger->GetTotalWaterStorageChanges()};
}

}  // namespace

TEST(ProcessET, LinearBuildsRunsAndClosesBalance) {
    SettingsModel model;
    BuildSoilModel(model, "et:linear");
    RunResults r = RunSoilModel(model);

    EXPECT_GT(r.et, 0.0);
    EXPECT_NEAR(r.discharge + r.et + r.storage - r.precip, 0.0, 1e-7);
}

TEST(ProcessET, PowerLawBuildsRunsAndClosesBalance) {
    SettingsModel model;
    BuildSoilModel(model, "et:power_law", "exponent", 0.5f);
    RunResults r = RunSoilModel(model);

    EXPECT_GT(r.et, 0.0);
    EXPECT_NEAR(r.discharge + r.et + r.storage - r.precip, 0.0, 1e-7);
}

TEST(ProcessET, ExponentialBuildsRunsAndClosesBalance) {
    SettingsModel model;
    BuildSoilModel(model, "et:exponential", "alpha", 2.0f);
    RunResults r = RunSoilModel(model);

    EXPECT_GT(r.et, 0.0);
    EXPECT_NEAR(r.discharge + r.et + r.storage - r.precip, 0.0, 1e-7);
}

TEST(ProcessET, PowerLawWithExponentHalfMatchesSocont) {
    SettingsModel socont;
    BuildSoilModel(socont, "et:socont");
    RunResults rSocont = RunSoilModel(socont);

    SettingsModel powerLaw;
    BuildSoilModel(powerLaw, "et:power_law", "exponent", 0.5f);
    RunResults rPowerLaw = RunSoilModel(powerLaw);

    EXPECT_NEAR(rPowerLaw.et, rSocont.et, 1e-8);
    EXPECT_NEAR(rPowerLaw.discharge, rSocont.discharge, 1e-8);
}

TEST(ProcessET, PowerLawWithExponentOneMatchesLinear) {
    SettingsModel linear;
    BuildSoilModel(linear, "et:linear");
    RunResults rLinear = RunSoilModel(linear);

    SettingsModel powerLaw;
    BuildSoilModel(powerLaw, "et:power_law", "exponent", 1.0f);
    RunResults rPowerLaw = RunSoilModel(powerLaw);

    EXPECT_NEAR(rPowerLaw.et, rLinear.et, 1e-8);
    EXPECT_NEAR(rPowerLaw.discharge, rLinear.discharge, 1e-8);
}
