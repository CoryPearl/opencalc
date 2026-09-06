if not sensors.available():
    print("Scientific I/O is disabled or missing")
else:
    sensors.rate(128)
    sensors.list_clear(1)
    graphics.clear(0)

    previous = sensors.analog_read(0)
    sensors.list_append(1, previous)
    for sample in range(1, 60):
        value = sensors.analog_read(0)
        sensors.list_append(1, value)

        y0 = 220 - int(previous * 60)
        y1 = 220 - int(value * 60)
        graphics.line((sample - 1) * 5, y0, sample * 5, y1, 65535)
        previous = value
        sensors.delay(50)

    print("Saved", sensors.list_count(1), "samples to L1")
