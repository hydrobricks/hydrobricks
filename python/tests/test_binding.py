import hydrobricks as hb


def test_main():
    param = hb.Parameter("super name")
    assert param.name == "super name"
