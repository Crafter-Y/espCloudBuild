# espCloudBuild

This project aims to compile an arduino esp32 on a github codespace (or similar). The actual project is irrelevant in that sense

## The actual project in here

The code in here is a LED thing that might not even work itself.

I started this project to see if I could cut the compile times by moving to a cloud based system

## Dependencies

This project includes:

- [AsyncTCP](https://github.com/ESP32Async/AsyncTCP.git)
- [ESPAsyncWebServer](https://github.com/ESP32Async/ESPAsyncWebServer)
- FastLED
- ArduinoJson

## Run in codespace

Everything should be set up to run:

```bash
arduino-cli compile --fqbn esp32:esp32:d1_mini32 espCloudBuild.ino --libraries ./lib --output-dir ./build
```

## Run in windows (without Docker)

Install Arduino [here](https://arduino.github.io/arduino-cli/latest/installation/)

Install esp32

```powershell
arduino-cli core update-index
arduino-cli core install esp32:esp32
```

```powershell
git submodule update --init --recursive
arduino-cli lib install FastLED
arduino-cli lib install ArduinoJson
```

run it:

```powershell
arduino-cli compile --fqbn esp32:esp32:d1_mini32 espCloudBuild.ino --libraries ./lib --output-dir ./build
```

## Building on GH Actions

This project gets automatically build by the GH Action infrastructure on every push to main.
The Artifacts can be downloaded from the Actions tab

## License

CC BY 4.0

espCloudBuild (c) by Crafter-Y

espCloudBuild is licensed under a
Creative Commons Attribution 4.0 International License.

You should have received a copy of the license along with this
work. If not, see <https://creativecommons.org/licenses/by/4.0/>.
