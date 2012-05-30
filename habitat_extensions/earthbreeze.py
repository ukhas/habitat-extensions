# Copyright 2012 EarhtBreeze Team, Daniel Richman

"""
Temperature sensor for EarthBreeze
"""

__all__ = ["temperature"]

def temperature(config, data):
    if len(data) > 2:
        raise ValueError("Value too short")

    prefix = data[0]
    suffix = data[-1]
    data = data[1:-1]

    if suffix != "C" or prefix not in ["P", "N"]:
        raise ValueError("Invalid prefix or suffix")

    data = int(data)
    if prefix == "N":
        data = 0 - data

    return data
