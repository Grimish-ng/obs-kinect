# obs-kinect (CMake / Cross-platform)

OBS Studio plugin to use a **Kinect v1 (Xbox 360)** as a camera source, with support for depth-based virtual green screen effects and infrared streaming.

This is a fork of [SirLynix/obs-kinect](https://github.com/SirLynix/obs-kinect) with:
- A native **CMake build system** replacing xmake (works on Linux, planned for Windows)
- Compatibility fixes for **libfreenect 0.7.x**
- **Infrared streaming** support with dynamic RGB/IR mode switching
- Proper **depth-to-RGB registration** via `FREENECT_DEPTH_REGISTERED`
- Tested on **Bazzite / Fedora 43** with Kinect v1 (model 1414)

> **Current status:** Linux fully working. Windows cmake support is planned — see [Roadmap](#roadmap).

---

## Supported Sources

| Source | Linux | Windows (planned) |
|---|---|---|
| Color (RGB) | ✅ Working | 🔲 Planned |
| Depth | ✅ Working (mm, registered to RGB) | 🔲 Planned |
| Infrared | ✅ Working (10-bit, scaled to 16-bit) | 🔲 Planned |
| Color-Mapped Depth | ✅ Working (aligned to RGB frame) | 🔲 Planned |
| Green Screen (depth-based) | ✅ Working | 🔲 Planned |
| Body/Skeletal tracking | 🔲 Planned (OpenNI2/NiTE2) | 🔲 Planned (Kinect SDK) |
| Dedicated background removal | ❌ Windows SDK only | 🔲 Planned (Kinect SDK) |

> **Note:** RGB and Infrared share the same hardware pipeline — switching between them causes a brief resync (normal USB isochronous behaviour). Depth streams independently and is unaffected.

---

## Supported Kinect Models

| Model | Status |
|---|---|
| 1414 (Xbox 360 Kinect original) | ✅ Confirmed working |
| 1473 (newer Xbox 360 Kinect) | ⚠️ RGB/Depth should work, tilt/LED may not |
| 1517 (Kinect for Windows v1) | ⚠️ RGB/Depth should work, tilt/LED may not |
| Kinect v2 (Xbox One) | ❌ Requires libfreenect2 backend (not built) |
| Azure Kinect | ❌ Windows/Azure SDK only |

---

## Requirements

- OBS Studio >= 25.0
- libfreenect >= 0.5
- libusb >= 1.0
- cmake >= 3.16
- A Kinect v1 (Xbox 360 or Kinect for Windows) with its powered USB adapter

### Install dependencies

**Fedora / Bazzite:**
```bash
rpm-ostree install libfreenect libfreenect-devel obs-studio-devel cmake
# reboot after
```

**Arch Linux:**
```bash
sudo pacman -S libfreenect obs-studio cmake
```

**Ubuntu / Debian:**
```bash
sudo apt install libfreenect-dev libobs-dev cmake build-essential libusb-1.0-0-dev
```

---

## udev Rules

The Kinect requires udev rules to be accessible without root. Install them once on the host system:

```bash
sudo wget https://raw.githubusercontent.com/OpenKinect/libfreenect/master/platform/linux/udev/51-kinect.rules \
  -O /etc/udev/rules.d/51-kinect.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
sudo usermod -aG video,plugdev $USER
```

Log out and back in for group changes to apply.

---

## Building

Clone this repository:

```bash
git clone https://github.com/Grimish-ng/obs-kinect.git
cd obs-kinect
git checkout linux-support
```

### User install (recommended)

Installs to `~/.config/obs-studio/plugins/obs-kinect/`:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
cmake --install build
```

### System-wide install

Installs to `/usr/lib/obs-plugins/` (requires sudo):

```bash
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
sudo cmake --install build
```

### Test install (dry run)

Verify install layout without touching your live OBS setup:

```bash
cmake --install build --prefix /tmp/obs-kinect-test
find /tmp/obs-kinect-test -type f | sort
```

---

## Usage

1. Plug in your Kinect using its powered USB adapter
2. Verify the device is detected:
   ```bash
   lsusb | grep -i xbox
   ```
   You should see three entries: NUI Camera, NUI Motor, NUI Audio
3. Launch OBS Studio
4. In the **Sources** panel click **+** → **Kinect**
5. Select your Kinect device and stream type:
   - **Color** — standard RGB camera
   - **Depth** — depth map in mm
   - **Infrared** — IR camera (switches hardware mode, RGB unavailable simultaneously)

### Green screen / background removal

The plugin supports depth-based virtual green screen — no physical green screen needed:

1. Add a Kinect source
2. Enable **Green Screen** in the source settings
3. Adjust **Min/Max distance** to mask out the background by depth
4. Depth values are in millimeters (e.g. 500–1500mm for a typical seated setup)

### Infrared mode

Selecting **Infrared** as the source type switches the Kinect's video pipeline from RGB to IR:
- Brief packet resync is normal during the switch
- IR values are 10-bit (0–1023), scaled to 16-bit for OBS
- Switch back to Color to restore RGB streaming
- Depth continues streaming independently during mode switches

---

## Troubleshooting

**Kinect not detected (`lsusb` shows nothing):**
- Make sure you're using the powered USB adapter, not bare USB
- Check udev rules are installed and you're in the `video` and `plugdev` groups

**Plugin not loading in OBS:**
- Check the OBS log: `~/.config/obs-studio/logs/`
- Make sure libfreenect is installed on the host (not just inside a container)
- On SELinux systems (Fedora/Bazzite), set the correct context:
  ```bash
  chcon -t textrel_shlib_t ~/.config/obs-studio/plugins/obs-kinect/bin/64bit/*.so
  ```

**Only NUI Motor detected, not Camera or Audio:**
- Your USB port may not be providing enough power — try a powered USB hub
- Try a different USB port

**Packet loss / "Invalid magic" in OBS log:**
- This is normal during IR/RGB mode switching and on initial connect
- libfreenect resyncs automatically

**Infrared greyed out:**
- Rebuild from this branch — earlier builds did not advertise `Source_Infrared`

---

## Roadmap

### Windows cmake support (v2.0)

The original project used xmake on Windows. The goal is to make the cmake build system work on Windows too, giving a single unified build system across all platforms.

What needs doing:

- **Windows install paths** — OBS plugins go in `%APPDATA%\obs-studio\plugins\` on Windows; the CMakeLists.txt needs to detect the platform and set the correct default prefix
- **Kinect SDK v1 backend** (`obs-kinect-sdk10`) — restore the Windows SDK v1 target in CMakeLists.txt with `find_package(KinectSDK10)` and MSVC-specific compile flags
- **Kinect SDK v2 backend** (`obs-kinect-sdk20`) — same for the Xbox One Kinect
- **MSVC flags** — re-add `/Zc:__cplusplus`, `/Zc:referenceBinding`, `/Zc:throwingNew`, `/wd4251` conditionally under `if(MSVC)`
- **libfreenect on Windows** — the freenect backend should also work on Windows since libfreenect supports it, though users need to install the libfreenect Windows binary

Contributions welcome — if you have a Windows machine with a Kinect and the Kinect SDK installed, this would be a great place to help.

### Body/skeletal tracking on Linux (v2.1)

The obs-kinect core already has infrastructure for `Source_Body` and `BodyIndexFrameData`. On Linux, skeletal tracking requires one of:

- **OpenNI2 + NiTE2** — NiTE2 is a closed-source middleware that provides 20-joint skeleton tracking on Kinect v1. OpenNI2-FreenectDriver bridges libfreenect to the OpenNI2 API. This is the most capable option but NiTE2 is an old proprietary binary blob.
- **Skeltrack** — open source skeleton tracker built on libfreenect by Igalia, purely depth-based, no proprietary dependencies, but less accurate and somewhat unmaintained.
- **MediaPipe / ML-based** — feed the Kinect RGB stream through a modern pose estimation model. Doesn't use depth for tracking but works with any camera and is actively maintained.

---

## What was changed from upstream

| File | Change |
|---|---|
| `CMakeLists.txt` | New — native cmake build replacing xmake. Distro-agnostic, supports user and system-wide install |
| `src/obs-kinect-freenect/FreenectDevice.cpp` | Full rewrite: infrared support, dynamic RGB/IR switching, `FREENECT_DEPTH_REGISTERED` for proper depth alignment, libfreenect 0.7.x API fixes |
| `src/obs-kinect/KinectPlugin.cpp` | Add `~/.config/obs-studio/plugins/obs-kinect/bin/64bit/` to backend search paths |
| `src/obs-kinect/KinectSource.cpp` | Add missing `#include <stdexcept>` |
| `src/obs-kinect/Shaders/*.cpp` | Add missing `#include <cstdint>` |
| `src/obs-kinect/GreenscreenEffects/*.cpp/.hpp` | Add missing `#include <cstdint>` |

---

## Release history

| Tag | Description |
|---|---|
| `v1.0-linux` | First working Linux build on Bazzite/Fedora 43 |
| `v1.1-linux` | Distro-agnostic libobs detection (Fedora/Arch/Ubuntu) |
| `v1.2-linux` | Proper cmake `--prefix` support, correct locale/effect install paths |
| `v1.3-linux` | Infrared support, registered depth, dynamic RGB/IR mode switching |
| `v1.4-linux` | Updated README, roadmap for Windows and body tracking |

---

## Contributing

Pull requests welcome, especially for:
- Windows cmake build support
- Testing on Kinect models 1473 and 1517
- OpenNI2/NiTE2 body tracking integration
- libfreenect2 backend for Kinect v2

---

## Original project

This plugin is originally by [SirLynix](https://github.com/SirLynix/obs-kinect). Please check the upstream repo for Windows, Azure Kinect, and Kinect v2 support.
