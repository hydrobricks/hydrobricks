def validate_kwargs(kwargs, allowed_kwargs):
    """Checks the keyword arguments against a set of allowed keys."""
    for kwarg in kwargs:
        if kwarg not in allowed_kwargs:
            raise TypeError('Keyword argument not understood:', kwarg)
