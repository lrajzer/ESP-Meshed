# ESP-Meshed
<!-- Cool logo lol -->
<!-- /$$$$$$$$  /$$$$$$  /$$$$$$$         /$$      /$$ /$$$$$$$$  /$$$$$$  /$$   /$$ /$$$$$$$$ /$$$$$$$  -->
<!-- | $$_____/ /$$__  $$| $$__  $$       | $$$    /$$$| $$_____/ /$$__  $$| $$  | $$| $$_____/| $$__  $$ -->
<!-- | $$      | $$  \__/| $$  \ $$       | $$$$  /$$$$| $$      | $$  \__/| $$  | $$| $$      | $$  \ $$ -->
<!-- | $$$$$   |  $$$$$$ | $$$$$$$//$$$$$$| $$ $$/$$ $$| $$$$$   |  $$$$$$ | $$$$$$$$| $$$$$   | $$  | $$ -->
<!-- | $$__/    \____  $$| $$____/|______/| $$  $$$| $$| $$__/    \____  $$| $$__  $$| $$__/   | $$  | $$ -->
<!-- | $$       /$$  \ $$| $$             | $$\  $ | $$| $$       /$$  \ $$| $$  | $$| $$      | $$  | $$ -->
<!-- | $$$$$$$$|  $$$$$$/| $$             | $$ \/  | $$| $$$$$$$$|  $$$$$$/| $$  | $$| $$$$$$$$| $$$$$$$/ -->
<!-- |________/ \______/ |__/             |__/     |__/|________/ \______/ |__/  |__/|________/|_______/  -->
This library was creted by [me](https://github.com/lrajzer) and [Bartek Szodny](https://github.com/BartlomiejSzkodny) for a research paper we wrote which is available [here](https://www.engineerxxi.ath.eu/publikacje/).

## How to use

First add a library to Your PlatformIO project, either using the [library registry](https://registry.platformio.org/libraries/rajzer/ESP%20Meshed) or by adding the lines below to Your `platformio.ini`

```platformio.ini
lib_deps = rajzer/ESP Meshed@^0.0.2
```

You can then include the library as any other

```c++
#include <esp-meshed.h>
```

To see exact usage, You can use the examples provided in the `examples` directory.
