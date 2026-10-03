## LED Cube {version}

A {type} build of [{repo}](https://github.com/{repo}), made {date} from `{short_sha}`.

### Updating a cube that is already running

With **GitHub Updates** switched on in the dashboard, the cube picks this release up on its own
(beta builds only when **Development Builds** is on as well). To update by hand, upload
`esp32-s3-devkitc-1-{version}.bin` from the dashboard. Use the application image, not the
`-factory` one.

`esp32s3.bin` is the same application image under the name older firmware looks for, so cubes
that have not updated since the move to these release names can still find it.

### Flashing a blank board

`esp32-s3-devkitc-1-{version}-factory.bin` is the bootloader, partition table and application
already merged, so you only need to flash one file.

Using [ESPConnect](https://thelastoutpostworkshop.github.io/ESPConnect/) (Chrome or Edge, board
connected over USB):

1. Click **Connect**, and pick the board's serial port.
2. Open the **Flash Tools** section.
3. Under **Flash Firmware**, choose `esp32-s3-devkitc-1-{version}-factory.bin` and leave the flash
   offset at `0x0`.
4. Click **Flash Firmware**.

### Sizes

{sizes}
