#include <gtest/gtest.h>

#include "SettingsModel.h"

TEST(SettingsModel, ParseYamlSocont) {
    SettingsModel settings;
    EXPECT_TRUE(settings.Parse("files/socont-2-stores-model.yaml"));
}
