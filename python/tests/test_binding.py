import hydrobricks as hb


def test_main():
    param = hb.Parameter("super name")
    assert param.get_name() == "super name"
