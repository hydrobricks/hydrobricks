#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "ModelHydro.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"

/**
 * Tests for the snow sublimation processes: sublimation:constant and
 * sublimation:pet.
 *
 * Both live on the snow container of a snowpack brick and remove snow water
 * equivalent directly to the atmosphere (like ET, they need no target brick). The
 * tests use a minimal snowpack fed by a cold precipitation event, then check that
 * the process runs, removes snow, and closes the water balance (the sublimated
 * amount is accounted for as an atmosphere/ET term).
 */
namespace {

/// Build a minimal snowpack model with the given sublimation process. A cold
/// precipitation event on day 1 builds the snowpack; the sublimation process then
/// removes snow to the atmosphere. paramName/paramValue set the process parameter
/// when it has one (empty paramName keeps the default).
void BuildSnowpackModel(SettingsModel& model, const std::string& subKind, const std::string& paramName = "",
                        float paramValue = 0.0f) {
    model.SetSolver("heun_explicit");
    model.SetTimer("2020-01-01", "2020-01-05", 1, "day");

    // Snowpack brick
    model.AddHydroUnitBrick("snowpack", "snowpack");
    model.AddBrickLogging({"snow_content"});

    // Sublimation process (atmosphere-bound, no target)
    model.AddBrickProcess("sublimation", subKind);
    if (!paramName.empty()) {
        model.SetProcessParameterValue(paramName, paramValue);
    }
    model.AddProcessLogging("output");

    // Rain/snow splitter
    model.AddHydroUnitSplitter("snow_rain", "snow_rain:linear");
    model.AddSplitterForcing("precipitation");
    model.AddSplitterForcing("temperature");
    model.AddSplitterOutput("outlet");                       // rain
    model.AddSplitterOutput("snowpack", ContentType::Snow);  // snow
    model.AddSplitterParameter("transition_start", 0.0f);
    model.AddSplitterParameter("transition_end", 2.0f);

    model.AddLoggingToItem("outlet");
}

std::unique_ptr<TimeSeriesUniform> MakeForcing(VariableType type, const vecDouble& values) {
    auto data = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 5), 1, TimeUnit::Day);
    data->SetValues(values);
    auto ts = std::make_unique<TimeSeriesUniform>(type);
    ts->SetData(std::move(data));
    return ts;
}

struct SublimationRun {
    double precip;
    double discharge;
    double et;
    double storage;
    double snowStorage;
    double firstSnow;
    double lastSnow;
};

/// Run the snowpack model with the given temperature forcing (and optional PET) on
/// a fixed 20 mm snow-on-day-1 event, and return the logger totals.
SublimationRun RunSnowpackModel(SettingsModel& settings, const vecDouble& temperature, const vecDouble* pet = nullptr) {
    SettingsBasin basinSettings;
    basinSettings.AddHydroUnit(1, 100);

    SubBasin subBasin;
    EXPECT_TRUE(subBasin.Initialize(basinSettings));

    ModelHydro model(&subBasin);
    EXPECT_TRUE(model.Initialize(settings, basinSettings));
    EXPECT_TRUE(model.IsValid());

    EXPECT_TRUE(model.AddTimeSeries(
        std::unique_ptr<TimeSeries>(MakeForcing(VariableType::Precipitation, {20.0, 0.0, 0.0, 0.0, 0.0}))));
    EXPECT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(MakeForcing(VariableType::Temperature, temperature))));
    if (pet != nullptr) {
        EXPECT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(MakeForcing(VariableType::PET, *pet))));
    }
    EXPECT_TRUE(model.AttachTimeSeriesToHydroUnits());

    EXPECT_TRUE(model.Run());

    Logger* logger = model.GetLogger();
    vecAxxd unitContent = logger->GetHydroUnitValues();  // [0] == snow_content
    double firstSnow = unitContent[0](0, 0);
    double lastSnow = unitContent[0](unitContent[0].rows() - 1, 0);

    return {20.0,
            logger->GetTotalOutletDischarge(),
            logger->GetTotalET(),
            logger->GetTotalWaterStorageChanges(),
            logger->GetTotalSnowStorageChanges(),
            firstSnow,
            lastSnow};
}

}  // namespace

TEST(ProcessSublimation, ConstantRemovesSnowAndClosesBalance) {
    SettingsModel model;
    BuildSnowpackModel(model, "sublimation:constant", "sublimation_rate", 2.0f);
    SublimationRun r = RunSnowpackModel(model, {-5.0, -5.0, -5.0, -5.0, -5.0});

    EXPECT_GT(r.et, 0.0);                 // snow was sublimated
    EXPECT_NEAR(r.discharge, 0.0, 1e-8);  // no melt, no rain
    EXPECT_LT(r.lastSnow, r.firstSnow);   // snowpack shrinks over time
    EXPECT_NEAR(r.discharge + r.et + r.storage + r.snowStorage - r.precip, 0.0, 1e-7);
}

TEST(ProcessSublimation, PetFractionRemovesSnowAndClosesBalance) {
    SettingsModel model;
    BuildSnowpackModel(model, "sublimation:pet", "sublimation_pet_factor", 1.0f);
    vecDouble pet = {2.0, 2.0, 2.0, 2.0, 2.0};
    SublimationRun r = RunSnowpackModel(model, {-5.0, -5.0, -5.0, -5.0, -5.0}, &pet);

    EXPECT_GT(r.et, 0.0);
    EXPECT_LT(r.lastSnow, r.firstSnow);
    EXPECT_NEAR(r.discharge + r.et + r.storage + r.snowStorage - r.precip, 0.0, 1e-7);
}
