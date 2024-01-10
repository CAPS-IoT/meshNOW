# MeshNOW - A Sensor Mesh Network over ESP-NOW

Create a mesh network of ESP32 microcontrollers that communicate using ESP-NOW.
This constellation allows for long-range communication and is ideal for sparsely covering a larger environment, e.g. outdoors.
Another perk of MeshNOW is the transparent forwarding of upper-level protocols. This way you can use any TCP-based application (e.g. MQTT) without manual intervention. Just like connecting to a normal WiFi AP.


This project was created by [Marvin Bauer](https://mrvnbr.de) as part of his Bachelor Thesis @ TUM under the supervision of Prof. Dr. Michael Gerndt and guidance of Isaac Núñez at the chair of Computer Architecture and Parallel Systems.

## Usage
Checkout [the docs](https://meshnow.mrvnbr.de) for a getting started guide.

(For installation, follow the below procedure as the documentation is out of date in that regard)

## Installation
*Note: MeshNOW has only been tested with with ESP-IDF version v5.0.1.*

This project uses the [IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html) for easy installation and dependency management.

For the component that you want to use MeshNOW in...
1. add a `idf_component.yml`: `idf.py create-manifest --component=main` (replace main with the component of your choosing)
2. Add the following entry under the `dependencies` key:
```yml
meshnow:
  git: "https://github.com/CAPS-IoT/meshNOW.git"
  path: "meshnow"
```
