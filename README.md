# EZ-FLASH Omega Definitive Edition Kernel

### How to build

The kernel is GBA homebrew built with **devkitARM** (devkitPro). A current
release works — GCC 16 builds clean; the original kernel targeted
devkitARM_r53.

Set `DEVKITARM`, `DEVKITPRO` and `LIBGBA` (and `PATH`) for your installation,
then run `make`. The build is just `make` on every platform:

- **Linux:**

      source /etc/profile.d/devkit-env.sh
      export LIBGBA=$DEVKITPRO/libgba
      make              # clean: make clean

- **Windows:** edit the paths in `build r53-32.bat` for your devkitPro
  install, then run it (it sets the variables and calls `make`).

The output is named after the project directory — e.g.
`ezflash-omega-de-kernel.gba` (the Makefile uses `TARGET := $(notdir $(CURDIR))`).

### Flashing

Link the built `.gba` together with `image.bin` into the kernel-upgrade `.bin`
using `Link_kernel_image.exe` (Windows; runs under Wine on Linux). That `.bin`
is the OmegaDE kernel-upgrade file you flash to the cart.

### Code quality

Compiler warnings (`-Wall -Wextra …`) and two local static-analysis passes
(GCC `-fanalyzer`, `cppcheck`) are documented in
[docs/CODE_QUALITY.md](docs/CODE_QUALITY.md).
