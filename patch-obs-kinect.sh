#!/bin/bash
# patch-obs-kinect.sh
# Patches obs-kinect source files for compatibility with libfreenect 0.7.5 on Linux
# Run from anywhere: bash patch-obs-kinect.sh ~/obs-kinect

KINECT_DIR="${1:-$HOME/obs-kinect}"

if [ ! -d "$KINECT_DIR" ]; then
    echo "Error: obs-kinect directory not found at $KINECT_DIR"
    echo "Usage: bash patch-obs-kinect.sh /path/to/obs-kinect"
    exit 1
fi

echo "Patching obs-kinect at: $KINECT_DIR"

# ── Patch 1: FreenectDevice.cpp ─────────────────────────────────────────────
# - Add missing #include <cstdint>
# - Switch depth mode from FREENECT_DEPTH_11BIT_PACKED to FREENECT_DEPTH_11BIT
#   (avoids needing freenect_convert_packed_to_16bit which doesn't exist in 0.7.5)
# - Replace freenect_convert_packed_to_16bit with memcpy
# - Replace freenect_map_depth_to_rgb (doesn't exist) with memcpy of raw depth

FILE="$KINECT_DIR/src/obs-kinect-freenect/FreenectDevice.cpp"
echo "  Patching $FILE"

# Add cstdint include after the existing cstring include
sed -i 's|#include <cstring>|#include <cstdint>\n#include <cstring>|' "$FILE"

# Switch packed depth mode to unpacked - driver will give us 16bit directly
sed -i 's|FREENECT_DEPTH_11BIT_PACKED|FREENECT_DEPTH_11BIT|g' "$FILE"

# Replace freenect_convert_packed_to_16bit(...) with memcpy
# Original: freenect_convert_packed_to_16bit(src, dst, 11, width*height);
# Replace with: memcpy(dst, src, memSize);
sed -i 's|freenect_convert_packed_to_16bit(reinterpret_cast<std::uint8_t\*>(frameMem), reinterpret_cast<std::uint16_t\*>(frameData.memory.data()), 11, frameData.width\*frameData.height);|std::memcpy(frameData.memory.data(), frameMem, memSize);|' "$FILE"

# Replace freenect_map_depth_to_rgb(...) with memcpy
# The color-mapped depth just copies raw depth data since we don't have this function
sed -i 's|freenect_map_depth_to_rgb(m_device, reinterpret_cast<std::uint8_t\*>(frameMem), reinterpret_cast<std::uint16_t\*>(frameData.memory.data()));|std::memcpy(frameData.memory.data(), frameMem, memSize);|' "$FILE"

echo "    Done"

# ── Patch 2: Add #include <cstdint> to shader/effect files missing it ────────

MISSING_CSTDINT=(
    "src/obs-kinect/Shaders/AlphaMaskShader.cpp"
    "src/obs-kinect/Shaders/VisibilityMaskShader.cpp"
    "src/obs-kinect/Shaders/GaussianBlurShader.cpp"
    "src/obs-kinect/Shaders/TextureLerpShader.cpp"
    "src/obs-kinect/GreenscreenEffects/ReplaceBackgroundEffect.cpp"
    "src/obs-kinect/GreenscreenEffects/ReplaceBackgroundEffect.hpp"
)

for f in "${MISSING_CSTDINT[@]}"; do
    FILE="$KINECT_DIR/$f"
    if [ -f "$FILE" ]; then
        if ! grep -q "#include <cstdint>" "$FILE"; then
            echo "  Adding <cstdint> to $f"
            # Insert after the first #include line
            sed -i '0,/#include/s|#include|#include <cstdint>\n#include|' "$FILE"
        fi
    fi
done

# ── Patch 3: KinectSource.cpp — add missing #include <stdexcept> ─────────────

FILE="$KINECT_DIR/src/obs-kinect/KinectSource.cpp"
echo "  Patching $FILE"
if ! grep -q "#include <stdexcept>" "$FILE"; then
    sed -i 's|#include <optional>|#include <optional>\n#include <stdexcept>|' "$FILE"
fi
echo "    Done"

echo ""
echo "All patches applied. Now rebuild:"
echo "  cd $KINECT_DIR/build"
echo "  rm -rf *"
echo "  cmake .. -Dlibobs_DIR=/usr/lib64/cmake/libobs"
echo "  make -j\$(nproc)"
