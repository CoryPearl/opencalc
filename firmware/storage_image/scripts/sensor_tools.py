def average(values):
    if len(values) == 0:
        raise ValueError("no sensor readings")
    return sum(values) / len(values)


def volts_to_percent(volts, maximum=3.3):
    percent = volts * 100 / maximum

    if percent < 0:
        return 0
    if percent > 100:
        return 100

    return percent