# Copyright 2012 Daniel Richman. GNU GPL 3

"""
Sensor functions for ints and floats that have a suffix on the end.
"""

__all__ = ["ascii_int", "ascii_float"]

def _suffixify(config, data):
    if "suffix" not in config:
        raise ValueError("No suffix key in config")

    suffix = config["suffix"]

    if len(data) <= 1 or data[-1] != suffix:
        raise ValueError("Incorrect Suffix")

    return data[:-1]

def ascii_int(config, data):
    data = _suffixify(config, data)

    if config.get("optional", False) and data == '':
        return None
    return int(data)

def ascii_float(config, data):
    data = _suffixify(config, data)

    if config.get("optional", False) and data == '':
        return None
    val = float(data)
    if math.isnan(val) or math.isinf(val):
        raise ValueError("Cannot accept nan, inf or -inf")
    return val
