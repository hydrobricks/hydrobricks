#include <gtest/gtest.h>
#include <wx/stdpaths.h>

#include "FileNetcdf.h"

TEST(FileNetcdf, FileGetsOpened) {
    FileNetcdf file;

    EXPECT_TRUE(file.OpenReadOnly("files/time-series-data.nc"));
}

TEST(FileNetcdf, FileGetsCreated) {
    FileNetcdf file;

    wxString path = wxStandardPaths::Get().GetTempDir() + "/hb_test_file.nc";
    EXPECT_TRUE(file.Create(path.ToStdString()));
    wxFile::Exists(path);
    file.Close();
    wxRemoveFile(path);
}

TEST(FileNetcdf, VarsNumberIsRead) {
    FileNetcdf file;

    ASSERT_TRUE(file.OpenReadOnly("files/time-series-data.nc"));

    EXPECT_EQ(file.GetVarsNb(), 5);
}

TEST(FileNetcdf, VarIdIsRead) {
    FileNetcdf file;

    ASSERT_TRUE(file.OpenReadOnly("files/time-series-data.nc"));

    EXPECT_EQ(file.GetVarId("time"), 1);
}

TEST(FileNetcdf, VarNameIsRead) {
    FileNetcdf file;

    ASSERT_TRUE(file.OpenReadOnly("files/time-series-data.nc"));

    EXPECT_TRUE(file.GetVarName(1) == "time");
}

TEST(FileNetcdf, VarDimIdsAreRead) {
    FileNetcdf file;

    ASSERT_TRUE(file.OpenReadOnly("files/time-series-data.nc"));

    vecInt dimsId = file.GetVarDimIds(2, 2);
    EXPECT_TRUE(file.GetVarName(2) == "temperature");

    EXPECT_EQ(dimsId[0], 1);
    EXPECT_EQ(dimsId[1], 0);
}

TEST(FileNetcdf, DimGetsDefined) {
    FileNetcdf file;

    wxString path = wxStandardPaths::Get().GetTempDir() + "/hb_test_file.nc";
    ASSERT_TRUE(file.Create(path.ToStdString()));
    wxFile::Exists(path);
    file.DefDim("dim1", 100);
    EXPECT_EQ(file.GetDimLen("dim1"), 100);
    file.Close();

    wxRemoveFile(path);
}

TEST(FileNetcdf, DimIdIsRead) {
    FileNetcdf file;

    ASSERT_TRUE(file.OpenReadOnly("files/time-series-data.nc"));

    EXPECT_EQ(file.GetDimId("time"), 1);
    EXPECT_EQ(file.GetDimId("hydro_units"), 0);
}

TEST(FileNetcdf, DimLengthIsRead) {
    FileNetcdf file;

    ASSERT_TRUE(file.OpenReadOnly("files/time-series-data.nc"));

    EXPECT_EQ(file.GetDimLen("hydro_units"), 100);
}

TEST(FileNetcdf, VarIntGetsDefined) {
    FileNetcdf file;

    wxString path = wxStandardPaths::Get().GetTempDir() + "/hb_test_file.nc";
    ASSERT_TRUE(file.Create(path.ToStdString()));
    ASSERT_TRUE(wxFile::Exists(path));
    int dim1Id = file.DefDim("dim1", 100);
    file.DefVarInt("var1", {dim1Id}, 1, false);
    file.DefVarInt("var2", {dim1Id}, 1, true);
    EXPECT_EQ(file.GetDimLen("dim1"), 100);
    file.Close();

    wxRemoveFile(path);
}

TEST(FileNetcdf, VarFloatGetsDefined) {
    FileNetcdf file;

    wxString path = wxStandardPaths::Get().GetTempDir() + "/hb_test_file.nc";
    ASSERT_TRUE(file.Create(path.ToStdString()));
    ASSERT_TRUE(wxFile::Exists(path));
    int dim1Id = file.DefDim("dim1", 100);
    file.DefVarFloat("var1", {dim1Id}, 1, false);
    file.DefVarFloat("var2", {dim1Id}, 1, true);
    EXPECT_EQ(file.GetDimLen("dim1"), 100);
    file.Close();

    wxRemoveFile(path);
}

TEST(FileNetcdf, VarDoubleGetsDefined) {
    FileNetcdf file;

    wxString path = wxStandardPaths::Get().GetTempDir() + "/hb_test_file.nc";
    ASSERT_TRUE(file.Create(path.ToStdString()));
    ASSERT_TRUE(wxFile::Exists(path));
    int dim1Id = file.DefDim("dim1", 100);
    file.DefVarDouble("var1", {dim1Id}, 1, false);
    file.DefVarDouble("var2", {dim1Id}, 1, true);
    EXPECT_EQ(file.GetDimLen("dim1"), 100);
    file.Close();

    wxRemoveFile(path);
}

TEST(FileNetcdf, VarInt1DIsRead) {
    FileNetcdf file;

    ASSERT_TRUE(file.OpenReadOnly("files/time-series-data.nc"));

    vecInt values = file.GetVarInt1D("id", 100);

    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[10], 11);
    EXPECT_EQ(values[50], 51);
    EXPECT_EQ(values[99], 100);
}

TEST(FileNetcdf, VarFloat1DIsRead) {
    FileNetcdf file;

    ASSERT_TRUE(file.OpenReadOnly("files/time-series-data.nc"));

    vecFloat values = file.GetVarFloat1D("time", 731);

    EXPECT_EQ(values[0], 56931.0);
    EXPECT_EQ(values[10], 56941.0);
    EXPECT_EQ(values[20], 56951.0);
    EXPECT_EQ(values[730], 57661.0);
}

TEST(FileNetcdf, VarDouble1DIsRead) {
    FileNetcdf file;

    wxString path = wxStandardPaths::Get().GetTempDir() + "/hb_test_file.nc";
    ASSERT_TRUE(file.Create(path.ToStdString()));
    ASSERT_TRUE(wxFile::Exists(path));
    int dim1Id = file.DefDim("dim1", 10);
    file.DefVarDouble("var2", {dim1Id}, 1, true);
    vecDouble valuesIn = {0.0001, 0.0002, 0.0003, 0.0004, 0.0005, 0.0006, 0.0007, 0.0008, 0.0009, 0.0010};
    file.PutVar(0, valuesIn);
    vecDouble valuesOut = file.GetVarDouble1D("var2", 10);
    file.Close();

    EXPECT_EQ(valuesOut[0], 0.0001);
    EXPECT_EQ(valuesOut[1], 0.0002);
    EXPECT_EQ(valuesOut[9], 0.0010);
}

TEST(FileNetcdf, VarDouble2DIsRead) {
    FileNetcdf file;

    ASSERT_TRUE(file.OpenReadOnly("files/time-series-data.nc"));

    int varId = file.GetVarId("temperature");
    axxd values = file.GetVarDouble2D(varId, 100, 731);

    EXPECT_NEAR(values(0, 0), 29.545898, 0.000001);
    EXPECT_NEAR(values(9, 0), 27.601563, 0.000001);
    EXPECT_NEAR(values(0, 9), 17.146484, 0.000001);
    EXPECT_NEAR(values(9, 9), 15.202148, 0.000001);
    EXPECT_NEAR(values(99, 730), 3.162109, 0.000001);
}

TEST(FileNetcdf, VarInt1DIsWritten) {
    FileNetcdf file;

    wxString path = wxStandardPaths::Get().GetTempDir() + "/hb_test_file.nc";
    ASSERT_TRUE(file.Create(path.ToStdString()));
    ASSERT_TRUE(wxFile::Exists(path));
    int dim1Id = file.DefDim("dim1", 10);
    file.DefVarInt("var2", {dim1Id}, 1, true);
    vecInt valuesIn = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    file.PutVar(0, valuesIn);
    vecInt valuesOut = file.GetVarInt1D("var2", 10);
    file.Close();

    EXPECT_EQ(valuesOut[0], 1);
    EXPECT_EQ(valuesOut[1], 2);
    EXPECT_EQ(valuesOut[9], 10);
}

TEST(FileNetcdf, VarFloat1DIsWritten) {
    FileNetcdf file;

    wxString path = wxStandardPaths::Get().GetTempDir() + "/hb_test_file.nc";
    ASSERT_TRUE(file.Create(path.ToStdString()));
    ASSERT_TRUE(wxFile::Exists(path));
    int dim1Id = file.DefDim("dim1", 10);
    file.DefVarFloat("var2", {dim1Id}, 1, true);
    vecFloat valuesIn = {0.0001f, 0.0002f, 0.0003f, 0.0004f, 0.0005f, 0.0006f, 0.0007f, 0.0008f, 0.0009f, 0.0010f};
    file.PutVar(0, valuesIn);
    vecFloat valuesOut = file.GetVarFloat1D("var2", 10);
    file.Close();

    EXPECT_NEAR(valuesOut[0], 0.0001, 0.00001);
    EXPECT_NEAR(valuesOut[1], 0.0002, 0.00001);
    EXPECT_NEAR(valuesOut[9], 0.0010, 0.00001);
}

TEST(FileNetcdf, VarDouble1DIsWritten) {
    FileNetcdf file;

    wxString path = wxStandardPaths::Get().GetTempDir() + "/hb_test_file.nc";
    ASSERT_TRUE(file.Create(path.ToStdString()));
    ASSERT_TRUE(wxFile::Exists(path));
    int dim1Id = file.DefDim("dim1", 10);
    file.DefVarDouble("var2", {dim1Id}, 1, true);
    vecDouble valuesIn = {0.0001, 0.0002, 0.0003, 0.0004, 0.0005, 0.0006, 0.0007, 0.0008, 0.0009, 0.0010};
    file.PutVar(0, valuesIn);
    vecDouble valuesOut = file.GetVarDouble1D("var2", 10);
    file.Close();

    EXPECT_EQ(valuesOut[0], 0.0001);
    EXPECT_EQ(valuesOut[1], 0.0002);
    EXPECT_EQ(valuesOut[9], 0.0010);
}

TEST(FileNetcdf, AttString1DIsRead) {
    FileNetcdf file;

    ASSERT_TRUE(file.OpenReadOnly("files/hydro-units-2-glaciers.nc"));

    vecStr landCovers = file.GetAttString1D("surface_names");

    EXPECT_EQ(landCovers.size(), 3);
    EXPECT_TRUE(landCovers[0] == "ground");
    EXPECT_TRUE(landCovers[1] == "glacier_ice");
    EXPECT_TRUE(landCovers[2] == "glacier_debris");
}

TEST(FileNetcdf, AttString1DIsWritten) {
    FileNetcdf file;

    wxString path = wxStandardPaths::Get().GetTempDir() + "/hb_test_file.nc";
    ASSERT_TRUE(file.Create(path.ToStdString()));
    ASSERT_TRUE(wxFile::Exists(path));
    int dim1Id = file.DefDim("dim1", 10);
    file.DefVarDouble("var1", {dim1Id}, 1, true);
    int varId = file.GetVarId("var1");
    vecStr valsIn = {"val1", "val2", "val3"};
    file.PutAttString("att_name", valsIn, varId);

    vecStr valsOut = file.GetAttString1D("att_name", "var1");

    EXPECT_EQ(valsOut.size(), 3);
    EXPECT_TRUE(valsOut[0] == "val1");
    EXPECT_TRUE(valsOut[1] == "val2");
    EXPECT_TRUE(valsOut[2] == "val3");
}

TEST(FileNetcdf, AttTextIsRead) {
    FileNetcdf file;

    ASSERT_TRUE(file.OpenReadOnly("files/hydro-units-2-glaciers.nc"));

    wxString type = file.GetAttText("type", "glacier_debris");

    EXPECT_TRUE(type.IsSameAs("glacier"));
}

TEST(FileNetcdf, AttTextIsWritten) {
    FileNetcdf file;

    wxString path = wxStandardPaths::Get().GetTempDir() + "/hb_test_file.nc";
    ASSERT_TRUE(file.Create(path.ToStdString()));
    ASSERT_TRUE(wxFile::Exists(path));
    int dim1Id = file.DefDim("dim1", 10);
    file.DefVarDouble("var1", {dim1Id}, 1, true);
    int varId = file.GetVarId("var1");
    file.PutAttText("att_name", "my_att", varId);

    wxString retrieved = file.GetAttText("att_name", "var1");

    file.Close();

    EXPECT_TRUE(retrieved.IsSameAs("my_att"));
}
