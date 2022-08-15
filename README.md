# DryNoMore

TODO description, images, etc.

## Software

### Get the sources
- `git clone --recursive https://github.com/7FM/DryNoMore.git`
- Or `git clone https://github.com/7FM/DryNoMore.git` followed by `git submodule update --init --recursive`

### Dependencies
<details>
  <summary>For Debian:</summary>

```
apt install git git-lfs gcc make binutils cmake libssl-dev libboost-system-dev zlib1g-dev libyaml-cpp-dev 
```
Note that you will need `gcc-11` to compile the telegram bot, as it uses `std::bit_cast` (added in C++20).
Debian bullseye uses `gcc-10` by default and `g++-11` is currently only available in the testing channel.
Hence, you must add 
```
deb http://httpredir.debian.org/debian testing main contrib non-free
```
to `/etc/apt/sources.list`.
To avoid updating all packages to the testing channel create the file `/etc/apt/preferences.d/unstable-preferences` with the following content:
```
Package: *
Pin: release  a=testing
Pin-Priority: 100

Package: *
Pin: release a=unstable
Pin-Priority: 200
```
Then you can use `apt -t testing install gcc` to install `gcc-11`.
</details>

For NixOS see [`flake.nix`](./flake.nix).

### Compilation
- For the Arduino project simply use PlatformIO, i.e. through the [PlatformIO IDE](https://platformio.org/install/ide?install=vscode).
- For the Telegram Bot simply go into the folder `server/telegram_bot` and run `make`. On success a binary should be present at `server/telegram_bot/build/drynomore-telegram-bot`

### Configuration

<details>
  <summary>Configure the Arduino Project:</summary>

- all major settings are adjustable in the [`config.hpp`](./include/config.hpp)

TODO list options
</details>
<details>
  <summary>Configure the Telegram Bot:</summary>

- all major settings are adjustable though a YAML config file. As an example take a look at the [`example_config.yaml`](./server/telegram_bot/example_config.yaml).

TODO list options
</details>

## Hardware
### Revisions:
- v0.1 known bugs:
    - Barrel Jack power input (J10) has swapped polarity.

- v0.2 known bugs:
