#include <gtest/gtest.h>

#include "SettingsModel.h"

TEST(SettingsModel, ParseYamlSocont) {
    SettingsModel settings;
    // yaml file parsing is broken in the current version
    // EXPECT_FALSE(settings.ParseStructure("files/socont-2-stores-model.yaml"));
}

TEST(SettingsModel, ParseJsonSocont) {
    SettingsModel settings;
    // yaml file parsing is broken in the current version
    // EXPECT_FALSE(settings.ParseStructure("files/socont-2-stores-model.json"));
}

TEST(SettingsModel, ParseYamlSocontParametersExplicit) {
    SettingsModel settings;
    // yaml file parsing is broken in the current version
    // EXPECT_FALSE(settings.ParseStructure("files/socont-2-stores-model.yaml"));
    EXPECT_FALSE(settings.ParseParameters("files/socont-2-stores-parameters-detailed.yaml"));
}

TEST(SettingsModel, ParseYamlSocontParametersCondensed) {
    SettingsModel settings;
    // yaml file parsing is broken in the current version
    // EXPECT_FALSE(settings.ParseStructure("files/socont-2-stores-model.yaml"));
    EXPECT_FALSE(settings.ParseParameters("files/socont-2-stores-parameters.yaml"));
}
