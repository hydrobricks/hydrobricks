import hydrobricks.models as models


def test_model_creation():
    model_i = models.Model("my model")
    assert model_i.name == "my model"


def test_socont_creation():
    model_i = models.Socont(soil_storage_nb=2)
    assert model_i.name == "Socont"
