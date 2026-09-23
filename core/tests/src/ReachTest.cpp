#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

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

    Fixture(const string& scheme, double lengthM, float celerity, float x = 0.2f, double slope = 0, double width = 0,
            double manning = 0, bool routeLocal = false) {
        SubbasinSettings subbasinSettings;
        subbasinSettings.id = 1;
        subbasinSettings.propertiesDouble.push_back({"length", lengthM, "m"});
        if (slope > 0) {
            subbasinSettings.propertiesDouble.push_back({"slope", slope, "m/m"});
        }
        if (width > 0) {
            subbasinSettings.propertiesDouble.push_back({"width", width, "m"});
        }
        if (manning > 0) {
            subbasinSettings.propertiesDouble.push_back({"manning", manning, ""});
        }
        subbasin.SetNetworkProperties(subbasinSettings);
        settings.SetChannelRouting(scheme, routeLocal);
        settings.SetParameterValue("channel", "celerity", celerity);
        settings.SetParameterValue("channel", "x", x);
        reach = std::make_unique<Reach>(&subbasin);
        reach->Initialize();
        reach->SetChannelRouting(settings.GetChannelRoutingSettings());
    }

    // The reference discharge sizes the Muskingum-Cunge sub reaches. Set it before the first step: the
    // count is fixed once the routing has started.
    void SetReferenceDischarge(float discharge) {
        settings.SetParameterValue("channel", "reference_discharge", discharge);
        reach->SetChannelRouting(settings.GetChannelRoutingSettings());
    }

    // One time step of the real loop: the upstream branch first, then the local runoff.
    std::pair<double, double> Step(double upstream, double local, double dt = kDay) const {
        double routedUpstream = reach->RouteUpstream(upstream, dt);
        double routedLocal = reach->RouteLocal(local, dt);
        return {routedUpstream, routedLocal};
    }

    // Route a pulse of the given volume at step 0, then zeros; returns the outflow series.
    vecDouble Pulse(double volume, int steps, double dt = kDay) const {
        vecDouble out;
        for (int t = 0; t < steps; ++t) {
            out.push_back(reach->RouteUpstream(t == 0 ? volume : 0.0, dt));
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
    f.reach->RouteUpstream(100.0, kDay);
    EXPECT_NEAR(f.reach->GetStorage(), 100.0, 1e-12);
    f.reach->RouteUpstream(50.0, kDay);
    EXPECT_NEAR(f.reach->GetStorage(), 150.0, 1e-12);
    f.reach->RouteUpstream(0.0, kDay);
    EXPECT_NEAR(f.reach->GetStorage(), 150.0, 1e-12);
    EXPECT_NEAR(f.reach->RouteUpstream(0.0, kDay), 100.0, 1e-12);
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
    EXPECT_THROW(f.reach->RouteUpstream(1.0, kDay), ModelConfigError);
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
    settings.SetChannelRouting("muskingum");
    Reach reach(&subbasin);
    reach.Initialize();
    reach.SetChannelRouting(settings.GetChannelRoutingSettings());
    EXPECT_DOUBLE_EQ(reach.GetCelerity(), 2.5);
    EXPECT_DOUBLE_EQ(reach.GetMuskingumX(), 0.1);
    EXPECT_NEAR(reach.GetTravelTimeInDays(), 1000.0 / (2.5 * kSecondsPerDay), 1e-15);
}

TEST(Reach, ParameterChangeIsPickedUpBetweenRuns) {
    Fixture f("lag", 2.0 * kSecondsPerDay, 1.0f);
    vecDouble out = f.Pulse(100.0, 4);
    EXPECT_DOUBLE_EQ(out[2], 100.0);

    f.reach->Reset();
    f.settings.SetParameterValue("channel", "celerity", 2.0f);  // one day now
    out = f.Pulse(100.0, 4);
    EXPECT_DOUBLE_EQ(out[1], 100.0);
    EXPECT_DOUBLE_EQ(out[2], 0.0);
}

TEST(Reach, ResetRestoresTheSavedState) {
    Fixture f("lag", 2.0 * kSecondsPerDay, 1.0f);
    f.reach->RouteUpstream(100.0, kDay);
    f.reach->SaveAsInitialState();
    f.reach->RouteUpstream(0.0, kDay);
    f.reach->RouteUpstream(0.0, kDay);
    EXPECT_NEAR(f.reach->GetStorage(), 0.0, 1e-12);
    f.reach->Reset();
    EXPECT_NEAR(f.reach->GetStorage(), 100.0, 1e-12);
    // The saved water was due two steps after its arrival: one step remains.
    EXPECT_NEAR(f.reach->RouteUpstream(0.0, kDay), 0.0, 1e-12);
    EXPECT_NEAR(f.reach->RouteUpstream(0.0, kDay), 100.0, 1e-12);
}

TEST(SettingsModel, ChannelRoutingParametersAreSettable) {
    SettingsModel settings;
    EXPECT_FALSE(settings.SetParameterValue("channel", "celerity", 2.0f));  // no scheme yet
    settings.SetChannelRouting("lag");
    EXPECT_TRUE(settings.SetParameterValue("channel", "celerity", 2.0f));
    EXPECT_TRUE(settings.SetParameterValue("channel", "manning", 0.03f));
    EXPECT_FALSE(settings.SetParameterValue("channel", "no_such_parameter", 1.0f));
    const ChannelRoutingSettings& routing = settings.GetChannelRoutingSettings();
    EXPECT_EQ(routing.scheme, "lag");
    EXPECT_FALSE(routing.routeLocalRunoff);
    ASSERT_EQ(routing.parameters.size(), 6);
    EXPECT_FLOAT_EQ(routing.parameters[0].GetValue(), 2.0f);

    settings.SetChannelRouting("muskingum_cunge", true);
    EXPECT_TRUE(settings.GetChannelRoutingSettings().routeLocalRunoff);
    // The parameters are created once: the reaches keep pointers into them.
    EXPECT_EQ(settings.GetChannelRoutingSettings().parameters.size(), 6);
}

TEST(Reach, ResetWithoutSavedStateKeepsTheScheduleAligned) {
    // A reset before any state was saved (the calibration loop: run, reset, run) must leave the delivery
    // schedule sized like the ordinates, not empty: the next routing wrote past its end otherwise.
    Fixture f("lag", 2.0 * kSecondsPerDay, 1.0f);
    vecDouble first = f.Pulse(100.0, 4);
    EXPECT_DOUBLE_EQ(first[2], 100.0);

    f.reach->Reset();
    EXPECT_DOUBLE_EQ(f.reach->GetStorage(), 0.0);
    vecDouble second = f.Pulse(100.0, 4);
    EXPECT_DOUBLE_EQ(second[0], 0.0);
    EXPECT_DOUBLE_EQ(second[2], 100.0);
    EXPECT_NEAR(Sum(second), 100.0, 1e-12);

    // The same for the Muskingum scheme.
    Fixture m("muskingum", 2.0 * kSecondsPerDay, 1.0f, 0.2f);
    vecDouble a = m.Pulse(100.0, 5);
    m.reach->Reset();
    vecDouble b = m.Pulse(100.0, 5);
    for (int t = 0; t < 5; ++t) {
        EXPECT_NEAR(b[t], a[t], 1e-12) << "t=" << t;
    }
}

// ---- Discharge-dependent celerity --------------------------------------------------------------

TEST(Reach, CelerityIsConstantByDefault) {
    Fixture f("lag", 2.0 * kSecondsPerDay, 1.0f);
    EXPECT_DOUBLE_EQ(f.reach->GetCelerityForDischarge(0.5), 1.0);
    EXPECT_DOUBLE_EQ(f.reach->GetCelerityForDischarge(50.0), 1.0);
}

TEST(Reach, CelerityFollowsThePowerLawWhenAnExponentIsSet) {
    Fixture f("lag", 2.0 * kSecondsPerDay, 1.0f);
    f.settings.SetParameterValue("channel", "celerity_exponent", 0.4f);
    f.settings.SetParameterValue("channel", "reference_discharge", 10.0f);
    // c = c_ref (Q / Q_ref)^0.4: the reference discharge gives the reference celerity.
    // The parameters are floats, hence the tolerance.
    EXPECT_NEAR(f.reach->GetCelerityForDischarge(10.0), 1.0, 1e-6);
    EXPECT_NEAR(f.reach->GetCelerityForDischarge(40.0), std::pow(4.0, 0.4), 1e-6);
    // A vanishing discharge keeps the reference celerity rather than a vanishing one.
    EXPECT_DOUBLE_EQ(f.reach->GetCelerityForDischarge(0.0), 1.0);
    // The celerity stays in a plausible range for a river.
    EXPECT_LE(f.reach->GetCelerityForDischarge(1e12), 10.0);
    EXPECT_GE(f.reach->GetCelerityForDischarge(1e-6), 0.05);
}

TEST(Reach, ADischargeDependentCelerityRoutesABigPulseFaster) {
    // 2 days of travel at the reference discharge; a pulse ten times larger travels faster.
    auto arrival = [](double volume) {
        Fixture f("lag", 2.0 * kSecondsPerDay, 1.0f);
        f.settings.SetParameterValue("channel", "celerity_exponent", 0.4f);
        f.settings.SetParameterValue("channel", "reference_discharge", 1.0f);
        vecDouble out = f.Pulse(volume, 6);
        double total = Sum(out);
        double weighted = 0;
        for (int t = 0; t < static_cast<int>(out.size()); ++t) {
            weighted += t * out[t];
        }
        return weighted / total;  // the mean arrival step
    };
    double small = arrival(86400.0);         // 1 m3/s
    double large = arrival(10.0 * 86400.0);  // 10 m3/s
    EXPECT_LT(large, small);
}

// ---- Muskingum-Cunge ---------------------------------------------------------------------------

TEST(Reach, MuskingumCungeNeedsTheReachSlope) {
    // The scheme is physically based: without the slope it cannot compute the wave celerity.
    EXPECT_THROW(Fixture("muskingum_cunge", 10000.0, 1.0f, 0.2f), ModelConfigError);
    EXPECT_NO_THROW(Fixture("muskingum_cunge", 10000.0, 1.0f, 0.2f, 0.005, 20.0, 0.035));
}

TEST(Reach, MuskingumCungeCelerityFollowsTheManningRelation) {
    Fixture f("muskingum_cunge", 10000.0, 1.0f, 0.2f, 0.005, 20.0, 0.035);
    EXPECT_EQ(f.reach->GetScheme(), Reach::Scheme::MuskingumCunge);
    EXPECT_DOUBLE_EQ(f.reach->GetWidth(), 20.0);
    EXPECT_DOUBLE_EQ(f.reach->GetManning(), 0.035);

    // Under Manning, the depth scales as Q^0.6 and the velocity (hence the celerity) as Q^0.4.
    double c1 = f.reach->GetCelerityForDischarge(10.0);
    double c4 = f.reach->GetCelerityForDischarge(40.0);
    EXPECT_NEAR(c4 / c1, std::pow(4.0, 0.4), 1e-6);
    // A plausible mountain-river wave speed, and faster than the mean velocity.
    EXPECT_GT(c1, 1.0);
    EXPECT_LT(c1, 4.0);

    // A gentler slope and a rougher bed both slow the wave down.
    Fixture gentle("muskingum_cunge", 10000.0, 1.0f, 0.2f, 0.0005, 20.0, 0.035);
    EXPECT_LT(gentle.reach->GetCelerityForDischarge(10.0), c1);
    Fixture rough("muskingum_cunge", 10000.0, 1.0f, 0.2f, 0.005, 20.0, 0.08);
    EXPECT_LT(rough.reach->GetCelerityForDischarge(10.0), c1);

    // A dry channel gives no information: the reference celerity is kept.
    EXPECT_DOUBLE_EQ(f.reach->GetCelerityForDischarge(0.0), 1.0);
}

TEST(Reach, MuskingumCungeConservesMassAndAttenuates) {
    Fixture f("muskingum_cunge", 20000.0, 1.0f, 0.2f, 0.002, 20.0, 0.035);
    vecDouble out = f.Pulse(10.0 * 86400.0, 40);
    for (double v : out) {
        EXPECT_GE(v, 0.0);
    }
    EXPECT_NEAR(Sum(out) + f.reach->GetStorage(), 10.0 * 86400.0, 1e-6);
    // The wave is delayed and spread: the first step carries less than the whole pulse.
    EXPECT_LT(out[0], 10.0 * 86400.0);
    EXPECT_GT(Sum(out), 0.0);
}

TEST(Reach, MuskingumCungeNeedsNoCelerityCalibration) {
    // The celerity parameter is irrelevant once the geometry drives the scheme: two reaches differing only
    // by it route the same pulse identically.
    Fixture a("muskingum_cunge", 20000.0, 1.0f, 0.2f, 0.002, 20.0, 0.035);
    Fixture b("muskingum_cunge", 20000.0, 5.0f, 0.4f, 0.002, 20.0, 0.035);
    vecDouble outA = a.Pulse(10.0 * 86400.0, 20);
    vecDouble outB = b.Pulse(10.0 * 86400.0, 20);
    for (size_t t = 0; t < outA.size(); ++t) {
        EXPECT_NEAR(outB[t], outA[t], 1e-9) << "t=" << t;
    }
}

TEST(Reach, MuskingumCungeTravelTimeIgnoresTheCelerityParameter) {
    // The reported travel time must be the one the geometry gives, not length / celerity: the celerity
    // parameter is meaningless for this scheme, and a report built on it would be quietly wrong.
    Fixture f("muskingum_cunge", 20000.0, 1.0f, 0.2f, 0.002, 20.0, 0.035);
    double celerity = f.reach->GetCelerityForDischarge(1.0);
    EXPECT_NE(celerity, 1.0);
    EXPECT_NEAR(f.reach->GetTravelTimeInDays(), 20000.0 / (celerity * kSecondsPerDay), 1e-12);

    // The schemes taking the celerity as a parameter define it at the reference discharge, so they are
    // unaffected.
    Fixture lag("lag", 20000.0, 2.0f);
    EXPECT_NEAR(lag.reach->GetTravelTimeInDays(), 20000.0 / (2.0 * kSecondsPerDay), 1e-12);
}

TEST(Reach, MuskingumCungeKeepsOneSubreachWhenTheWaveIsResolved) {
    // A 20 km reach and a daily step: the wave crosses far more than the reach in one step, so a single
    // element resolves it and nothing changes.
    Fixture f("muskingum_cunge", 20000.0, 1.0f, 0.2f, 0.002, 20.0, 0.035);
    f.Pulse(10.0 * kSecondsPerDay, 2);
    EXPECT_EQ(f.reach->GetSubreachCount(), 1);
}

TEST(Reach, MuskingumCungeSplitsAReachTheWaveCannotCrossInAStep) {
    // The same reach at an hourly step: the wave travels ~2 km per step, so the reach must be divided.
    constexpr double kHour = 1.0 / 24.0;
    Fixture f("muskingum_cunge", 20000.0, 1.0f, 0.2f, 0.002, 20.0, 0.035);
    f.Pulse(10.0 * 3600.0, 2, kHour);

    // Ponce and Theurer: dx <= (c dt + q / (S0 c)) / 2, at the reference discharge (1 m3/s by default).
    double celerity = f.reach->GetCelerityForDischarge(1.0);
    double maxLength = 0.5 * (celerity * 3600.0 + (1.0 / 20.0) / (0.002 * celerity));
    int expected = static_cast<int>(std::ceil(20000.0 / maxLength));
    EXPECT_GT(expected, 1);
    EXPECT_EQ(f.reach->GetSubreachCount(), expected);
}

TEST(Reach, MuskingumCungeSubreachesAttenuateTheWaveTheSingleElementMisses) {
    // Two identical reaches: only the reference discharge differs, and it only sizes the sub reaches. A
    // large one leaves a single element, whose weighting factor tends to 0.5 over a long reach, i.e. pure
    // translation: it misses the diffusion the sub reaches reproduce.
    constexpr double kHour = 1.0 / 24.0;
    constexpr double kVolume = 10.0 * 3600.0;
    Fixture divided("muskingum_cunge", 20000.0, 1.0f, 0.2f, 0.002, 20.0, 0.035);
    Fixture single("muskingum_cunge", 20000.0, 1.0f, 0.2f, 0.002, 20.0, 0.035);
    single.SetReferenceDischarge(1.0e5f);

    vecDouble outDivided = divided.Pulse(kVolume, 120, kHour);
    vecDouble outSingle = single.Pulse(kVolume, 120, kHour);
    EXPECT_GT(divided.reach->GetSubreachCount(), 1);
    EXPECT_EQ(single.reach->GetSubreachCount(), 1);

    // Both conserve mass, and neither sends water backwards.
    EXPECT_NEAR(Sum(outDivided) + divided.reach->GetStorage(), kVolume, 1e-6);
    EXPECT_NEAR(Sum(outSingle) + single.reach->GetStorage(), kVolume, 1e-6);
    for (double v : outDivided) {
        EXPECT_GE(v, 0.0);
    }

    // The divided reach spreads the wave more: a lower peak.
    double peakDivided = *std::max_element(outDivided.begin(), outDivided.end());
    double peakSingle = *std::max_element(outSingle.begin(), outSingle.end());
    EXPECT_LT(peakDivided, peakSingle);
}

TEST(Reach, MuskingumCungeSubreachCountIsCapped) {
    // A very long and very flat reach at an hourly step would ask for thousands of sub reaches.
    constexpr double kHour = 1.0 / 24.0;
    Fixture f("muskingum_cunge", 500000.0, 1.0f, 0.2f, 0.0001, 20.0, 0.035);
    f.Pulse(10.0 * 3600.0, 2, kHour);
    EXPECT_EQ(f.reach->GetSubreachCount(), 50);
}

TEST(Reach, MuskingumCungeSubreachCountFollowsNewParameters) {
    // The reference discharge is calibratable: a new value between two runs must resize the sub reaches,
    // not keep the count the first run settled on.
    constexpr double kHour = 1.0 / 24.0;
    Fixture f("muskingum_cunge", 20000.0, 1.0f, 0.2f, 0.002, 20.0, 0.035);
    f.Pulse(10.0 * 3600.0, 2, kHour);
    int divided = f.reach->GetSubreachCount();
    EXPECT_GT(divided, 1);

    f.SetReferenceDischarge(1.0e5f);
    f.reach->Reset();
    f.Pulse(10.0 * 3600.0, 2, kHour);
    EXPECT_EQ(f.reach->GetSubreachCount(), 1);
}

TEST(Reach, MuskingumCungeSubreachesSurviveAReset) {
    // The sub reach state is a vector per branch: it must stay sized like the count across a reset, saved
    // state or not, or the next routing writes past its end.
    constexpr double kHour = 1.0 / 24.0;
    constexpr double kVolume = 10.0 * 3600.0;
    Fixture f("muskingum_cunge", 20000.0, 1.0f, 0.2f, 0.002, 20.0, 0.035);
    vecDouble first = f.Pulse(kVolume, 60, kHour);
    EXPECT_GT(f.reach->GetSubreachCount(), 1);

    // Reset without a saved state: the run starts again from an empty reach.
    f.reach->Reset();
    EXPECT_DOUBLE_EQ(f.reach->GetStorage(), 0.0);
    vecDouble again = f.Pulse(kVolume, 60, kHour);
    for (size_t t = 0; t < first.size(); ++t) {
        EXPECT_NEAR(again[t], first[t], 1e-9) << "t=" << t;
    }

    // And with a state saved mid-wave, the reset restores what the reach held, sub reach by sub reach.
    f.Pulse(kVolume, 5, kHour);
    double storage = f.reach->GetStorage();
    EXPECT_GT(storage, 0.0);
    f.reach->SaveAsInitialState();
    vecDouble after = f.Pulse(0.0, 30, kHour);
    f.reach->Reset();
    EXPECT_NEAR(f.reach->GetStorage(), storage, 1e-9);
    vecDouble repeated = f.Pulse(0.0, 30, kHour);
    for (size_t t = 0; t < after.size(); ++t) {
        EXPECT_NEAR(repeated[t], after[t], 1e-9) << "t=" << t;
    }
}

// ---- Local runoff ------------------------------------------------------------------------------

TEST(Reach, LocalRunoffJoinsAtTheOutletByDefault) {
    Fixture f("lag", 2.0 * kSecondsPerDay, 1.0f);
    EXPECT_FALSE(f.reach->RoutesLocalRunoff());
    auto [upstream, local] = f.Step(0.0, 100.0);
    EXPECT_DOUBLE_EQ(local, 100.0);  // unchanged, no travel time
    EXPECT_DOUBLE_EQ(upstream, 0.0);
    EXPECT_DOUBLE_EQ(f.reach->GetInflow(), 0.0);  // the local runoff never enters the reach
}

TEST(Reach, LocalRunoffTravelsHalfTheReach) {
    // Two days of travel over the whole reach, so one day over the half the local runoff sees.
    Fixture f("lag", 2.0 * kSecondsPerDay, 1.0f, 0.2f, 0, 0, 0, true);
    EXPECT_TRUE(f.reach->RoutesLocalRunoff());

    vecDouble local;
    vecDouble upstream;
    for (int t = 0; t < 5; ++t) {
        auto [up, loc] = f.Step(t == 0 ? 100.0 : 0.0, t == 0 ? 50.0 : 0.0);
        upstream.push_back(up);
        local.push_back(loc);
    }
    // The upstream pulse arrives after two steps, the local one after a single step.
    EXPECT_DOUBLE_EQ(upstream[2], 100.0);
    EXPECT_DOUBLE_EQ(local[1], 50.0);
    EXPECT_DOUBLE_EQ(local[0], 0.0);
    EXPECT_NEAR(Sum(local), 50.0, 1e-12);
    EXPECT_NEAR(Sum(upstream), 100.0, 1e-12);
}

TEST(Reach, LocalRunoffIsPartOfTheReachWaterBalance) {
    Fixture f("lag", 2.0 * kSecondsPerDay, 1.0f, 0.2f, 0, 0, 0, true);
    f.Step(100.0, 50.0);
    // Both branches entered the reach and are still in transit.
    EXPECT_DOUBLE_EQ(f.reach->GetInflow(), 150.0);
    EXPECT_DOUBLE_EQ(f.reach->GetOutflow(), 0.0);
    EXPECT_NEAR(f.reach->GetStorage(), 150.0, 1e-12);
    f.Step(0.0, 0.0);
    EXPECT_NEAR(f.reach->GetStorage(), 100.0, 1e-12);  // the local half delivered
}

TEST(Reach, LocalRunoffRoutingIsIgnoredWithoutAScheme) {
    Fixture f("none", 2.0 * kSecondsPerDay, 1.0f, 0.2f, 0, 0, 0, true);
    EXPECT_FALSE(f.reach->RoutesLocalRunoff());
    auto [upstream, local] = f.Step(100.0, 50.0);
    EXPECT_DOUBLE_EQ(upstream, 100.0);
    EXPECT_DOUBLE_EQ(local, 50.0);
}
