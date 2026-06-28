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

`make` also assembles the flashable kernel-upgrade image `ezkernelnew.bin`: the
built `.gba` zero-padded to 1 MiB, with the bundled `image.bin` top firmware
appended — the fixed layout the OmegaDE flasher and kernel expect. Flash that
`ezkernelnew.bin` to the cart. This native step replaces the old closed-source
`Link_kernel_image.exe` and is byte-identical to its output.

### Code quality

Compiler warnings (`-Wall -Wextra …`) and two local static-analysis passes
(GCC `-fanalyzer`, `cppcheck`) are documented in
[docs/CODE_QUALITY.md](docs/CODE_QUALITY.md).
