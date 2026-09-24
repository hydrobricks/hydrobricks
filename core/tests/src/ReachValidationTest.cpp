/**
 * Validation of the channel routing against the analytical solutions of the equations the schemes claim to
 * solve. These tests pin the physics rather than the implementation: they are written from the equations, not
 * from recorded output, so a refactoring that changed the answers would fail them.
 *
 * - The Muskingum scheme is a discretization of the linear storage S = K [X I + (1 - X) O]. With a constant
 *   inflow on an empty reach that ordinary differential equation solves to O(t) = I0 (1 - exp(-t / tau)) with
 *   tau = K (1 - X), so the routed volume after T is I0 [T - tau (1 - exp(-T / tau))]. The scheme must
 *   approach it as the time step shrinks.
 * - Muskingum-Cunge is built so that the numerical diffusion of the scheme reproduces the physical diffusion
 *   of the diffusion wave equation, dQ/dt + c dQ/dx = D d2Q/dx2, with the kinematic celerity c and the
 *   hydraulic diffusivity D = Q / (2 B S0). The first two moments of the impulse response of that equation
 *   are exact: the centroid is delayed by L / c and the variance grows by 2 D L / c^3.
 *
 * The Muskingum-Cunge comparison only holds where the reference does: the equation is linear, so the test
 * perturbs a steady base flow by a fraction of a percent, and it is a wave equation, so the time step must
 * resolve the wave. Section 'Inspecting the network' of the documentation records the accuracy measured
 * outside that window.
 */

#include <gtest/gtest.h>

#include <cmath>
#include <numeric>

#include "Reach.h"
#include "SettingsBasin.h"
#include "SettingsModel.h"
#include "SubBasin.h"

namespace {

constexpr double kSecondsPerDay = 86400.0;

/**
 * A reach with the given geometry, built from the settings exactly as the model builds it.
 */
struct ValidationReach {
    SubBasin subbasin;
    SettingsModel settings;
    std::unique_ptr<Reach> reach;

    ValidationReach(const string& scheme, double lengthM, double slope, double widthM, double manning, float celerity,
                    float x, float referenceDischarge) {
        SubbasinSettings subbasinSettings;
        subbasinSettings.id = 1;
        subbasinSettings.propertiesDouble.push_back({"length", lengthM, "m"});
        if (slope > 0) {
            subbasinSettings.propertiesDouble.push_back({"slope", slope, "m/m"});
        }
        if (widthM > 0) {
            subbasinSettings.propertiesDouble.push_back({"width", widthM, "m"});
        }
        if (manning > 0) {
            subbasinSettings.propertiesDouble.push_back({"manning", manning, ""});
        }
        subbasin.SetNetworkProperties(subbasinSettings);
        settings.SetChannelRouting(scheme, false);
        settings.SetParameterValue("channel", "celerity", celerity);
        settings.SetParameterValue("channel", "x", x);
        settings.SetParameterValue("channel", "reference_discharge", referenceDischarge);
        reach = std::make_unique<Reach>(&subbasin);
        reach->Initialize();
        reach->SetChannelRouting(settings.GetChannelRoutingSettings());
    }
};

/** The volume-weighted mean of the times of a series of volumes, sampled at the centre of each step [s]. */
double MeanTime(const vecDouble& volumes, double stepInSeconds) {
    double total = std::accumulate(volumes.begin(), volumes.end(), 0.0);
    double weighted = 0;
    for (size_t k = 0; k < volumes.size(); ++k) {
        weighted += (static_cast<double>(k) + 0.5) * stepInSeconds * volumes[k];
    }

    return weighted / total;
}

/** The volume-weighted variance of the times of a series of volumes [s2]. */
double VarianceOfTime(const vecDouble& volumes, double stepInSeconds) {
    double total = std::accumulate(volumes.begin(), volumes.end(), 0.0);
    double mean = MeanTime(volumes, stepInSeconds);
    double weighted = 0;
    for (size_t k = 0; k < volumes.size(); ++k) {
        double time = (static_cast<double>(k) + 0.5) * stepInSeconds;
        weighted += (time - mean) * (time - mean) * volumes[k];
    }

    return weighted / total;
}

/**
 * Bring a reach to steady state under a base flow, then perturb it by one short pulse and collect the
 * perturbation of the inflow and of the outflow, step by step. Perturbing a steady flow, rather than routing
 * a wave down an empty channel, is what makes the linear reference applicable.
 */
void RoutePerturbation(Reach& reach, double baseDischarge, double pulseFactor, double stepInDays, int steps,
                       vecDouble& inflow, vecDouble& outflow) {
    double stepInSeconds = stepInDays * kSecondsPerDay;
    double baseVolume = baseDischarge * stepInSeconds;
    for (int t = 0; t < 1200; ++t) {
        reach.RouteUpstream(baseVolume, stepInDays);
    }
    for (int t = 0; t < steps; ++t) {
        double volume = baseVolume * (t == 0 ? pulseFactor : 1.0);
        inflow.push_back(volume - baseVolume);
        outflow.push_back(reach.RouteUpstream(volume, stepInDays) - baseVolume);
    }
}

}  // namespace

TEST(ReachValidation, MuskingumSolvesTheLinearReservoir) {
    // X = 0 reduces the storage to S = K O, the linear reservoir, for which the scheme is the trapezoidal
    // rule: the error must fall by four when the step is halved.
    constexpr double kTravelTime = 2.0 * kSecondsPerDay;  // s, with a celerity of 1 m/s
    constexpr double kDischarge = 100.0;                  // m3/s
    constexpr double kHorizon = 6.0 * kSecondsPerDay;     // s

    double previous = 0;
    for (double stepInDays : {1.0, 0.5, 0.25, 0.125}) {
        ValidationReach f("muskingum", kTravelTime, 0, 0, 0, 1.0f, 0.0f, 1.0f);
        double stepInSeconds = stepInDays * kSecondsPerDay;
        int steps = static_cast<int>(std::lround(kHorizon / stepInSeconds));
        double routed = 0;
        for (int t = 0; t < steps; ++t) {
            routed += f.reach->RouteUpstream(kDischarge * stepInSeconds, stepInDays);
        }

        double expected = kDischarge * (kHorizon - kTravelTime * (1.0 - std::exp(-kHorizon / kTravelTime)));
        double error = std::abs(routed - expected) / expected;
        if (previous > 0) {
            EXPECT_GT(previous / error, 3.5) << "step = " << stepInDays << " d";
        }
        previous = error;
    }
    EXPECT_LT(previous, 1e-4);
}

TEST(ReachValidation, MuskingumSolvesTheLinearStorageWithAWeightingFactor) {
    // With X > 0 the scheme is only well posed for 2 K X <= dt <= 2 K (1 - X); below the lower bound its
    // first response would be negative and is clamped, which costs an order of accuracy but still converges.
    constexpr double kTravelTime = 2.0 * kSecondsPerDay;
    constexpr double kX = 0.2;
    constexpr double kTau = kTravelTime * (1.0 - kX);
    constexpr double kDischarge = 100.0;
    constexpr double kHorizon = 6.0 * kSecondsPerDay;

    double previous = 1.0;
    for (double stepInDays : {1.0, 0.5, 0.25, 0.125, 0.0625}) {
        ValidationReach f("muskingum", kTravelTime, 0, 0, 0, 1.0f, static_cast<float>(kX), 1.0f);
        double stepInSeconds = stepInDays * kSecondsPerDay;
        int steps = static_cast<int>(std::lround(kHorizon / stepInSeconds));
        double routed = 0;
        for (int t = 0; t < steps; ++t) {
            routed += f.reach->RouteUpstream(kDischarge * stepInSeconds, stepInDays);
        }

        double expected = kDischarge * (kHorizon - kTau * (1.0 - std::exp(-kHorizon / kTau)));
        double error = std::abs(routed - expected) / expected;
        EXPECT_LT(error, previous) << "step = " << stepInDays << " d";
        previous = error;
    }
    EXPECT_LT(previous, 0.01);
}

TEST(ReachValidation, MuskingumCungeMatchesTheDiffusionWaveMoments) {
    // A wide channel carrying a steady 50 m3/s, perturbed by a tenth of a percent so that the celerity and
    // the diffusivity stay those of the base flow, at a step resolving the wave (dt / K = 0.07).
    constexpr double kLength = 20000.0;  // m
    constexpr double kSlope = 0.001;
    constexpr double kWidth = 20.0;  // m
    constexpr double kManning = 0.035;
    constexpr double kBaseDischarge = 50.0;                 // m3/s
    constexpr double kStepInDays = 600.0 / kSecondsPerDay;  // 10 minutes

    ValidationReach f("muskingum_cunge", kLength, kSlope, kWidth, kManning, 1.0f, 0.2f, kBaseDischarge);
    vecDouble inflow, outflow;
    RoutePerturbation(*f.reach, kBaseDischarge, 1.001, kStepInDays, 400, inflow, outflow);

    // The perturbation comes out whole.
    double added = std::accumulate(inflow.begin(), inflow.end(), 0.0);
    double left = std::accumulate(outflow.begin(), outflow.end(), 0.0);
    EXPECT_NEAR(left / added, 1.0, 0.005);

    double stepInSeconds = kStepInDays * kSecondsPerDay;
    double celerity = f.reach->GetCelerityForDischarge(kBaseDischarge);
    double diffusivity = kBaseDischarge / (2.0 * kWidth * kSlope);
    double expectedLag = kLength / celerity;
    double expectedSpread = 2.0 * diffusivity * kLength / std::pow(celerity, 3.0);

    double lag = MeanTime(outflow, stepInSeconds) - MeanTime(inflow, stepInSeconds);
    double spread = VarianceOfTime(outflow, stepInSeconds) - VarianceOfTime(inflow, stepInSeconds);

    // The wave arrives when the celerity says, and has spread by what the diffusivity says.
    EXPECT_NEAR(lag / expectedLag, 1.0, 0.01) << "lag = " << lag << " s, expected " << expectedLag;
    EXPECT_NEAR(spread / expectedSpread, 1.0, 0.05) << "variance = " << spread << " s2, expected " << expectedSpread;
}
