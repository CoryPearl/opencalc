import time
import board
import digitalio
import analogio
import busio
import storage
import sensors
import sensor_tools as tools

led = digitalio.DigitalInOut(board.D0)
probe = analogio.AnalogIn(board.A0)
i2c = busio.I2C()

led.switch_to_output(False)

samples = []
sensors.list_clear(1)

try:
    print("Scientific I/O demo")
    print("I2C device at 0x76:", i2c.present(118))

    index = 0

    while index < 40:
        volts = probe.voltage()
        samples.append(volts)
        sensors.list_append(1, volts)

        led.write(index % 2 == 0)

        print(index, round(volts, 3), "V")
        storage.append(
            "sensor_log.txt",
            f"{index}, {volts}\n"
        )

        time.sleep(0.1)
        index = index + 1

    mean = tools.average(samples)
    percent = tools.volts_to_percent(mean)

    print("Average:", round(mean, 3), "V")
    print("Percent of 3.3 V:", round(percent, 1))
    print("Saved to calculator list L1")

except ValueError as error:
    print("Value error:", error)

except RuntimeError as error:
    print("Hardware error:", error)

finally:
    led.write(False)
    led.deinit()
    probe.deinit()
    i2c.deinit()
    print("Finished")