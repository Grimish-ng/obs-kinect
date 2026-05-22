# obs-kinect (Linux / libfreenect)

OBS Studio plugin to use a **Kinect v1 (Xbox 360)** as a camera source on Linux, with support for depth-based virtual green screen effects.

This is a fork of [SirLynix/obs-kinect](https://github.com/SirLynix/obs-kinect) with:
- A native **CMake build system** for Linux (replaces xmake)
- Compatibility fixes for **libfreenect 0.7.x**
- Tested on **Bazzite / Fedora 43** with Kinect v1 (model 1414)

> **Note:** Only the Kinect v1 (Xbox 360) freenect backend is supported on Linux. The Windows SDK, Azure Kinect, and Kinect v2 backends are not built.

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
5. Select your Kinect device and stream type (Color, Depth, or Infrared)

### Green screen / background removal

The plugin supports depth-based virtual green screen — no physical green screen needed:

1. Add a Kinect source
2. Enable **Green Screen** in the source settings
3. Adjust **Min/Max distance** to mask out the background by depth

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

---

## What was changed from upstream

| File | Change |
|---|---|
| `CMakeLists.txt` | New — native Linux cmake build, replaces xmake |
| `src/obs-kinect-freenect/FreenectDevice.cpp` | Switch to `FREENECT_DEPTH_11BIT`, replace missing API functions with `memcpy` |
| `src/obs-kinect/KinectPlugin.cpp` | Add `~/.config/obs-studio/plugins/obs-kinect/bin/64bit/` to backend search paths |
| `src/obs-kinect/KinectSource.cpp` | Add missing `#include <stdexcept>` |
| `src/obs-kinect/Shaders/*.cpp` | Add missing `#include <cstdint>` |
| `src/obs-kinect/GreenscreenEffects/*.cpp/.hpp` | Add missing `#include <cstdint>` |
| `patch-obs-kinect.sh` | Helper script to apply all source patches |

---

## Original project

This plugin is originally by [SirLynix](https://github.com/SirLynix/obs-kinect). Please check the upstream repo for Windows, Azure Kinect, and Kinect v2 support.
