#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "ModelHydro.h"
#include "SettingsModel.h"
#include "TimeSeriesUniform.h"

/**
 * routing:delay is a pure translation: the water reaching the routing brick leaves it
 * unchanged, a fixed duration later. The delay is a duration in days, so it shifts by
 * the same time at any step: a whole number of steps is an exact shift, and a shorter
 * delay moves the matching share of a step's water onto the next step.
 *
 * The tests mirror its use behind PREVAH's glacier reservoirs: a linear reservoir drains
 * either straight to the outlet or through the delay. With no delay the two must be the
 * same run, which is what keeps the delay brick from adding a spurious step.
 */
class RoutingDelay : public ::testing::TestWithParam<std::string> {
  protected:
    // A pulse of 24 mm falls on a linear reservoir during the first step; the reservoir
    // drains to the outlet directly (withDelay = false) or through a delay [d]. Returns
    // the outlet discharge per step [mm].
    static axd Run(bool withDelay, double delayInDays, int stepInHours, const std::string& solver) {
        SettingsModel settings;
        settings.SetLogAll(true);
        settings.SetSolver(solver);
        settings.SetTimer("2020-01-01", "2020-01-06", stepInHours, "hour");

        settings.AddHydroUnitBrick("reservoir", "storage");
        settings.AddBrickForcing("precipitation");
        settings.AddBrickProcess("outflow", "outflow:linear");
        settings.SetProcessParameterValue("response_factor", 0.8f);
        settings.AddProcessOutput(withDelay ? "delay" : "outlet");

        if (withDelay) {
            settings.AddHydroUnitBrick("delay", "storage");
            settings.AddBrickProcess("delay", "routing:delay");
            settings.SetProcessParameterValue("delay", static_cast<float>(delayInDays));
            settings.AddProcessOutput("outlet");
        }
        settings.AddLoggingToItem("outlet");

        int stepsPerDay = 24 / stepInHours;
        vecDouble values(5 * stepsPerDay + 1, 0.0);
        values[0] = 24.0;
        auto data = std::make_unique<TimeSeriesDataRegular>(GetMJD(2020, 1, 1), GetMJD(2020, 1, 6), stepInHours,
                                                            TimeUnit::Hour);
        data->SetValues(values);
        auto precip = std::make_unique<TimeSeriesUniform>(VariableType::Precipitation);
        precip->SetData(std::move(data));

        SettingsBasin basinSettings;
        basinSettings.AddHydroUnit(1, 100);
        SubBasin subBasin;
        EXPECT_TRUE(subBasin.Initialize(basinSettings));

        ModelHydro model(&subBasin);
        EXPECT_TRUE(model.Initialize(settings, basinSettings));
        EXPECT_TRUE(model.AddTimeSeries(std::unique_ptr<TimeSeries>(std::move(precip))));
        EXPECT_TRUE(model.AttachTimeSeriesToHydroUnits());
        EXPECT_TRUE(model.Run());

        return model.GetLogger()->GetOutletDischarge();
    }
};

TEST_P(RoutingDelay, NoDelayChangesNothing) {
    for (int hours : {1, 24}) {
        axd direct = Run(false, 0.0, hours, GetParam());
        axd delayed = Run(true, 0.0, hours, GetParam());
        ASSERT_EQ(direct.size(), delayed.size());
        for (Eigen::Index i = 0; i < direct.size(); ++i) {
            EXPECT_NEAR(delayed(i), direct(i), 1e-9) << "step " << i << ", " << hours << " h";
        }
    }
}

TEST_P(RoutingDelay, WholeStepsShiftTheHydrographExactly) {
    // Two hours on an hourly grid and one day on a daily one: the same hydrograph, two
    // steps (respectively one) later.
    for (auto [hours, steps] : {std::pair{1, 2}, std::pair{24, 1}}) {
        double delay = steps * hours / 24.0;
        axd direct = Run(false, 0.0, hours, GetParam());
        axd delayed = Run(true, delay, hours, GetParam());
        for (Eigen::Index i = 0; i + steps < direct.size(); ++i) {
            EXPECT_NEAR(delayed(i + steps), direct(i), 1e-9) << "step " << i << ", " << hours << " h";
        }
        for (Eigen::Index i = 0; i < steps; ++i) {
            EXPECT_NEAR(delayed(i), 0.0, 1e-9);
        }
    }
}

TEST_P(RoutingDelay, ASubStepDelaySharesEachStepWithTheNext) {
    // One hour on a daily grid: each day's outflow is a block one day long; shifted by an
    // hour, 23/24 of it stays in its day and 1/24 moves to the next one. The delay is a
    // float parameter, so the split carries its ~1e-8 relative rounding.
    axd direct = Run(false, 0.0, 24, GetParam());
    axd delayed = Run(true, 1.0 / 24.0, 24, GetParam());
    constexpr double tolerance = 1e-6;
    EXPECT_NEAR(delayed(0), direct(0) * 23.0 / 24.0, tolerance);
    for (Eigen::Index i = 1; i < direct.size(); ++i) {
        EXPECT_NEAR(delayed(i), direct(i) * 23.0 / 24.0 + direct(i - 1) / 24.0, tolerance) << "day " << i;
    }
}

TEST_P(RoutingDelay, AnySubDayDelayShiftsTheWaterByItsShareOfTheDay) {
    // On a daily grid a delay of h hours keeps (24 - h)/24 of each day's outflow in its
    // day and moves h/24 to the next: nothing is lost or created, only moved by h hours.
    axd direct = Run(false, 0.0, 24, GetParam());
    for (double hours : {5.0, 13.0, 23.0}) {
        axd delayed = Run(true, hours / 24.0, 24, GetParam());
        double moved = hours / 24.0;
        EXPECT_NEAR(delayed(0), direct(0) * (1.0 - moved), 1e-6) << hours << " h";
        for (Eigen::Index i = 1; i < direct.size(); ++i) {
            EXPECT_NEAR(delayed(i), direct(i) * (1.0 - moved) + direct(i - 1) * moved, 1e-6)
                << "day " << i << ", " << hours << " h";
        }
    }
}

// PREVAH runs the analytic solver; the explicit one is the plainest reference.
INSTANTIATE_TEST_SUITE_P(Solvers, RoutingDelay, ::testing::Values("analytic_linear", "euler_explicit"));
