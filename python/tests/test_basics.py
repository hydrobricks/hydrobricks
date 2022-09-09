import pytest
from hydrobricks.models.socont import Socont


def test_module_was_imported():
    model = Socont()
    assert model.get_name() == "Socont"

