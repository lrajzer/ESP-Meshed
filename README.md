# ESP-Meshed
<!-- Cool logo lol -->
<!--  /$$$$$$$$  /$$$$$$  /$$$$$$$         /$$      /$$ /$$$$$$$$  /$$$$$$  /$$   /$$ /$$$$$$$$ /$$$$$$$  -->
<!-- | $$_____/ /$$__  $$| $$__  $$       | $$$    /$$$| $$_____/ /$$__  $$| $$  | $$| $$_____/| $$__  $$ -->
<!-- | $$      | $$  \__/| $$  \ $$       | $$$$  /$$$$| $$      | $$  \__/| $$  | $$| $$      | $$  \ $$ -->
<!-- | $$$$$   |  $$$$$$ | $$$$$$$//$$$$$$| $$ $$/$$ $$| $$$$$   |  $$$$$$ | $$$$$$$$| $$$$$   | $$  | $$ -->
<!-- | $$__/    \____  $$| $$____/|______/| $$  $$$| $$| $$__/    \____  $$| $$__  $$| $$__/   | $$  | $$ -->
<!-- | $$       /$$  \ $$| $$             | $$\  $ | $$| $$       /$$  \ $$| $$  | $$| $$      | $$  | $$ -->
<!-- | $$$$$$$$|  $$$$$$/| $$             | $$ \/  | $$| $$$$$$$$|  $$$$$$/| $$  | $$| $$$$$$$$| $$$$$$$/ -->
<!-- |________/ \______/ |__/             |__/     |__/|________/ \______/ |__/  |__/|________/|_______/  -->
This library was creted by [me](https://github.com/lrajzer) and [Bartek Szodny](https://github.com/BartlomiejSzkodny) for a research paper we wrote which is available [here](https://www.engineerxxi.ath.eu/publikacje/).

## Using this library

### Installation

First add a library to Your PlatformIO project, either using the [library registry](https://registry.platformio.org/libraries/rajzer/ESP%20Meshed) or by adding the lines below to Your `platformio.ini`

```platformio.ini
lib_deps = rajzer/ESP Meshed@^0.0.2
```

You can then include the library as any other

```c++
#include <esp-meshed.h>
```

### How to use

The library creates a singleton class with its own access method.

```c++
ESPMeshedNode* node = GetESPMeshedNode();
```

To send messages simply use the `sendMessage` method, it takes in a 12-bit adress, and up-to 245 bits of data.

```c++
int data[5] = {1, 2, 3, 4, 5};
int data_length = 5;
node->sendMessage(*data, data_length, peer_adress);
```

To establish what happends when a message is received, You can set a callback either when getting the instance, or by using the setter.

```c++
void recvHandler(uint8_t *data, uint8_t len, uint16_t from)
{
    // Something happens to Your data here...
}
node->setReceiveHandler(recvHandler);
```

### Examples

To see more ways to use this library please checkout the examples provided in the `examples` directory.
