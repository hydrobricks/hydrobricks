#include <gtest/gtest.h>

#include <cmath>
#include <numeric>

#include "Reach.h"
#include "SettingsBasin.h"
#include "SettingsModel.h"
#include "SubBasin.h"

namespace {

constexpr double kDay = 1.0;
constexpr double kSecondsPerDay = 86400.0;

/**
 * A sub basin carrying a reach of the given length, and the model-wide routing settings.
 */
struct Fixture {
    SubBasin subbasin;
    SettingsModel settings;
    std::unique_ptr<Reach> reach;

    Fixture(const string& scheme, double lengthM, float celerity, float x = 0.2f) {
        SubbasinSettings subbasinSettings;
        subbasinSettings.id = 1;
        subbasinSettings.propertiesDouble.push_back({"length", lengthM, "m"});
        subbasin.SetNetworkProperties(subbasinSettings);
        settings.SetRouting(scheme);
        settings.SetParameterValue("routing", "celerity", celerity);
        settings.SetParameterValue("routing", "x", x);
        reach = std::make_unique<Reach>(&subbasin);
        reach->Initialize();
        reach->SetRouting(settings.GetRoutingSettings());
    }

    // Route a pulse of the given volume at step 0, then zeros; returns the outflow series.
    vecDouble Pulse(double volume, int steps, double dt = kDay) const {
        vecDouble out;
        for (int t = 0; t < steps; ++t) {
            out.push_back(reach->Route(t == 0 ? volume : 0.0, dt));
        }
        return out;
    }
};

double Sum(const vecDouble& values) {
    return std::accumulate(values.begin(), values.end(), 0.0);
}

}  // namespace

TEST(Reach, SchemeNames) {
    EXPECT_EQ(Reach::SchemeFromString("none"), Reach::Scheme::None);
    EXPECT_EQ(Reach::SchemeFromString(""), Reach::Scheme::None);
    EXPECT_EQ(Reach::SchemeFromString("lag"), Reach::Scheme::Lag);
    EXPECT_EQ(Reach::SchemeFromString("muskingum"), Reach::Scheme::Muskingum);
    EXPECT_THROW(Reach::SchemeFromString("kinematic"), ModelConfigError);
}

TEST(Reach, NoneIsInstantaneous) {
    Fixture f("none", 86400.0, 1.0f);
    vecDouble out = f.Pulse(100.0, 3);
    EXPECT_DOUBLE_EQ(out[0], 100.0);
    EXPECT_DOUBLE_EQ(out[1], 0.0);
    EXPECT_DOUBLE_EQ(f.reach->GetStorage(), 0.0);
}

TEST(Reach, TravelTimeFromLengthAndCelerity) {
    Fixture f("lag", 2.0 * kSecondsPerDay, 1.0f);  // 172.8 km at 1 m/s: two days
    EXPECT_NEAR(f.reach->GetTravelTimeInDays(), 2.0, 1e-12);
    EXPECT_DOUBLE_EQ(f.reach->GetCelerity(), 1.0);
    EXPECT_EQ(f.reach->GetScheme(), Reach::Scheme::Lag);
}

TEST(Reach, LagShiftsAPulseByWholeSteps) {
    Fixture f("lag", 2.0 * kSecondsPerDay, 1.0f);
    vecDouble out = f.Pulse(100.0, 5);
    EXPECT_DOUBLE_EQ(out[0], 0.0);
    EXPECT_DOUBLE_EQ(out[1], 0.0);
    EXPECT_DOUBLE_EQ(out[2], 100.0);
    EXPECT_DOUBLE_EQ(out[3], 0.0);
    EXPECT_NEAR(Sum(out), 100.0, 1e-12);
    EXPECT_DOUBLE_EQ(f.reach->GetStorage(), 0.0);
}

TEST(Reach, LagSplitsAFractionalTravelTime) {
    Fixture f("lag", 1.5 * kSecondsPerDay, 1.0f);  // 1.5 days: half in step 1, half in step 2
    vecDouble out = f.Pulse(100.0, 4);
    EXPECT_NEAR(out[0], 0.0, 1e-12);
    EXPECT_NEAR(out[1], 50.0, 1e-9);
    EXPECT_NEAR(out[2], 50.0, 1e-9);
    EXPECT_NEAR(Sum(out), 100.0, 1e-12);
}

TEST(Reach, LagWithoutLengthIsInstantaneous) {
    Fixture f("lag", 0.0, 1.0f);
    vecDouble out = f.Pulse(100.0, 2);
    EXPECT_DOUBLE_EQ(out[0], 100.0);
}

TEST(Reach, LagHourlyStepGivesTheSameDailyArrival) {
    // A two-day travel time on an hourly step delivers the pulse 48 hours later, in one hour.
    Fixture f("lag", 2.0 * kSecondsPerDay, 1.0f);
    vecDouble out = f.Pulse(100.0, 72, kDay / 24.0);
    EXPECT_DOUBLE_EQ(out[47], 0.0);
    EXPECT_NEAR(out[48], 100.0, 1e-9);
    EXPECT_NEAR(Sum(out), 100.0, 1e-12);
}

TEST(Reach, LagStorageTracksWaterInTransit) {
    Fixture f("lag", 3.0 * kSecondsPerDay, 1.0f);
    f.reach->Route(100.0, kDay);
    EXPECT_NEAR(f.reach->GetStorage(), 100.0, 1e-12);
    f.reach->Route(50.0, kDay);
    EXPECT_NEAR(f.reach->GetStorage(), 150.0, 1e-12);
    f.reach->Route(0.0, kDay);
    EXPECT_NEAR(f.reach->GetStorage(), 150.0, 1e-12);
    EXPECT_NEAR(f.reach->Route(0.0, kDay), 100.0, 1e-12);
    EXPECT_NEAR(f.reach->GetStorage(), 50.0, 1e-12);
}

TEST(Reach, MuskingumMatchesTheAnalyticalCoefficients) {
    // K = 2 days, X = 0.2, dt = 1 day: within 2KX = 0.8 <= dt <= 2K(1-X) = 3.2, so no sub-stepping.
    Fixture f("muskingum", 2.0 * kSecondsPerDay, 1.0f, 0.2f);
    double k = 2.0, dt = 1.0;
    double x = static_cast<double>(0.2f);  // the parameter is a float
    double denominator = 2 * k * (1 - x) + dt;
    double c0 = (dt - 2 * k * x) / denominator;
    double c1 = (dt + 2 * k * x) / denominator;
    double c2 = (2 * k * (1 - x) - dt) / denominator;

    vecDouble out = f.Pulse(100.0, 30);
    double expected0 = c0 * 100.0;
    double expected1 = c1 * 100.0 + c2 * expected0;
    double expected2 = c2 * expected1;
    EXPECT_NEAR(out[0], expected0, 1e-9);
    EXPECT_NEAR(out[1], expected1, 1e-9);
    EXPECT_NEAR(out[2], expected2, 1e-9);
    EXPECT_NEAR(out[3], c2 * expected2, 1e-9);

    // Mass: what left plus what is still in transit is the pulse.
    EXPECT_NEAR(Sum(out) + f.reach->GetStorage(), 100.0, 1e-9);
    EXPECT_NEAR(f.reach->GetStorage(), 0.0, 1e-6);  // 30 days: the recession is over
}

TEST(Reach, MuskingumShortReachSubstepsAndConservesMass) {
    // K = 0.1 day, X = 0.2: 2K(1-X) = 0.16 < dt, so the step is split into 7 sub-steps.
    Fixture f("muskingum", 0.1 * kSecondsPerDay, 1.0f, 0.2f);
    vecDouble out = f.Pulse(100.0, 10);
    for (double v : out) {
        EXPECT_GE(v, 0.0);
    }
    EXPECT_GT(out[0], 50.0);  // most of the pulse leaves within the step
    EXPECT_NEAR(Sum(out) + f.reach->GetStorage(), 100.0, 1e-9);
}

TEST(Reach, MuskingumVeryShortReachIsInstantaneous) {
    Fixture f("muskingum", 10.0, 1.0f, 0.2f);  // 10 s of travel time
    vecDouble out = f.Pulse(100.0, 2);
    EXPECT_DOUBLE_EQ(out[0], 100.0);
}

TEST(Reach, MuskingumNeverGoesNegativeWhenTheStepIsShort) {
    // K = 5 days, X = 0.4: 2KX = 4 > dt, so C0 < 0; the outflow is clamped at zero and mass is kept.
    Fixture f("muskingum", 5.0 * kSecondsPerDay, 1.0f, 0.4f);
    vecDouble out = f.Pulse(100.0, 60);
    for (double v : out) {
        EXPECT_GE(v, 0.0);
    }
    EXPECT_DOUBLE_EQ(out[0], 0.0);
    EXPECT_NEAR(Sum(out) + f.reach->GetStorage(), 100.0, 1e-9);
}

TEST(Reach, MuskingumRejectsAnInvalidWeightingFactor) {
    Fixture f("muskingum", 2.0 * kSecondsPerDay, 1.0f, 0.7f);
    EXPECT_THROW(f.reach->Route(1.0, kDay), ModelConfigError);
}

TEST(Reach, SubbasinPropertiesOverrideTheParameters) {
    SubBasin subbasin;
    SubbasinSettings subbasinSettings;
    subbasinSettings.id = 3;
    subbasinSettings.propertiesDouble.push_back({"length", 1000.0, "m"});
    subbasinSettings.propertiesDouble.push_back({"celerity", 2.5, "m/s"});
    subbasinSettings.propertiesDouble.push_back({"muskingum_x", 0.1, ""});
    subbasin.SetNetworkProperties(subbasinSettings);
    SettingsModel settings;
    settings.SetRouting("muskingum");
    Reach reach(&subbasin);
    reach.Initialize();
    reach.SetRouting(settings.GetRoutingSettings());
    EXPECT_DOUBLE_EQ(reach.GetCelerity(), 2.5);
    EXPECT_DOUBLE_EQ(reach.GetMuskingumX(), 0.1);
    EXPECT_NEAR(reach.GetTravelTimeInDays(), 1000.0 / (2.5 * kSecondsPerDay), 1e-15);
}

TEST(Reach, ParameterChangeIsPickedUpBetweenRuns) {
    Fixture f("lag", 2.0 * kSecondsPerDay, 1.0f);
    vecDouble out = f.Pulse(100.0, 4);
    EXPECT_DOUBLE_EQ(out[2], 100.0);

    f.reach->Reset();
    f.settings.SetParameterValue("routing", "celerity", 2.0f);  // one day now
    out = f.Pulse(100.0, 4);
    EXPECT_DOUBLE_EQ(out[1], 100.0);
    EXPECT_DOUBLE_EQ(out[2], 0.0);
}

TEST(Reach, ResetRestoresTheSavedState) {
    Fixture f("lag", 2.0 * kSecondsPerDay, 1.0f);
    f.reach->Route(100.0, kDay);
    f.reach->SaveAsInitialState();
    f.reach->Route(0.0, kDay);
    f.reach->Route(0.0, kDay);
    EXPECT_NEAR(f.reach->GetStorage(), 0.0, 1e-12);
    f.reach->Reset();
    EXPECT_NEAR(f.reach->GetStorage(), 100.0, 1e-12);
    // The saved water was due two steps after its arrival: one step remains.
    EXPECT_NEAR(f.reach->Route(0.0, kDay), 0.0, 1e-12);
    EXPECT_NEAR(f.reach->Route(0.0, kDay), 100.0, 1e-12);
}

TEST(SettingsModel, RoutingParametersAreSettable) {
    SettingsModel settings;
    EXPECT_FALSE(settings.SetParameterValue("routing", "celerity", 2.0f));  // no scheme yet
    settings.SetRouting("lag");
    EXPECT_TRUE(settings.SetParameterValue("routing", "celerity", 2.0f));
    EXPECT_FALSE(settings.SetParameterValue("routing", "manning", 0.03f));
    const RoutingSettings& routing = settings.GetRoutingSettings();
    EXPECT_EQ(routing.scheme, "lag");
    ASSERT_EQ(routing.parameters.size(), 2);
    EXPECT_FLOAT_EQ(routing.parameters[0].GetValue(), 2.0f);
}
