# LED Cube

[![build-release](https://github.com/elliotmatson/LED_Cube/actions/workflows/build-release.yml/badge.svg)](https://github.com/elliotmatson/LED_Cube/actions/workflows/build-release.yml)

An ESP32 based LED cube, inspired by [this project](https://github.com/Staacks/there.oughta.be/tree/master/led-cube).

# Development

## Setting up Dev Environment
To build firmware for this cube, all you need is an IDE and PlatformIO. I use VSCode, but you can use whatever you want. See [here](https://docs.platformio.org/en/latest/integration/ide/vscode.html#quick-start) for more details on setting up PlatformIO in VSCode.

Clone this repo, and open it in your IDE. You should be able to move on to building.

## Building
This project uses ESPIDF with Arduino as a component, and PlatformIO to build and upload firmware. To build, just click the build button in your IDE. To upload, click the upload button. You can also use the PlatformIO CLI to build and upload. See [here](https://docs.platformio.org/en/latest/core/userguide/cmd_build.html) for more details.

## Uploading
Currently, the cubes support 3 different methods of getting new firmware. 

### Github
The first one, used exclusively in production, is downloading straight from Github releases. These builds are created with Github Actions and are not used for development. 

### OTA
The second method is using OTA updates. This is the easiest method of uploading new firmware during development. To upload using OTA, you need to have the cube connected to your network. Ensure that these two lines are uncommented in platformio.ini, and that the cube is on the same network as your computer:

```ini
upload_protocol = espota
upload_port = cube.local
```

Then, click the upload button in your IDE. This will upload the firmware to the cube over the network. The cube will then reboot and start running the new firmware. 

### Serial
The third method is using a USB to serial adapter. This is mostly only useful if you have uploaded firmware that breaks OTA, bur can also be used for debugging. To upload using serial, you need to have the cube connected to your computer via USB. Like a fool, I didn't add silkscreen labels for RX and TX, so here is a labeled image:
![Screenshot 2022-12-29 at 4 50 48 PM](https://user-images.githubusercontent.com/1711604/210018862-ec6aab7d-4b61-4290-a931-6ad4d5c13356.png)


Connect the Cube GND to the adaptor GND, the Cube TX to the adaptor RX, and the Cube RX to the adaptor TX, then comment out these two lines in platformio.ini:

```ini
;upload_protocol = espota
;upload_port = cube.local
```
    
Then, click the upload button in your IDE. This will upload the firmware to the cube over serial. The cube will then reboot and start running the new firmware.
