#include <gtest/gtest.h>

#include "SettingsModel.h"

TEST(SettingsModel, ParseYamlSocont) {
    SettingsModel settings;
    EXPECT_TRUE(settings.ParseStructure("files/socont-2-stores-model.yaml"));
}

TEST(SettingsModel, ParseJsonSocont) {
    SettingsModel settings;
    EXPECT_TRUE(settings.ParseStructure("files/socont-2-stores-model.json"));
}

TEST(SettingsModel, ParseYamlSocontParametersExplicit) {
    SettingsModel settings;
    EXPECT_TRUE(settings.ParseStructure("files/socont-2-stores-model.yaml"));
    EXPECT_TRUE(settings.ParseParameters("files/socont-2-stores-parameters-detailed.yaml"));
}

TEST(SettingsModel, ParseYamlSocontParametersCondensed) {
    SettingsModel settings;
    EXPECT_TRUE(settings.ParseStructure("files/socont-2-stores-model.yaml"));
    EXPECT_TRUE(settings.ParseParameters("files/socont-2-stores-parameters.yaml"));
}
