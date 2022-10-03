from hydrobricks.models import base_model, socont


def test_model_creation():
    model_i = base_model.Model("my model")
    assert model_i.name == "my model"


def test_socont_creation():
    model_i = socont.Socont(soil_storage_nb=2)
    assert model_i.name == "Socont"
