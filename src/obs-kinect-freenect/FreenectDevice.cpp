/******************************************************************************
	Copyright (C) 2021 by Jérôme Leclercq <lynix680@gmail.com>

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.

	Linux/libfreenect modifications:
	- Dynamic RGB/IR video mode switching based on requested sources
	- Infrared support via FREENECT_VIDEO_IR_10BIT
	- Depth via FREENECT_DEPTH_REGISTERED (mm, aligned to RGB frame)
	- Fixes for libfreenect 0.7.x API compatibility
******************************************************************************/

#include "FreenectDevice.hpp"
#include <libfreenect/libfreenect_registration.h>
#include <util/threading.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <sstream>
#include <stdexcept>

KinectFreenectDevice::KinectFreenectDevice(freenect_device* device, const char* serial) :
m_device(device)
{
	SetSupportedSources(Source_Color | Source_Depth | Source_ColorMappedDepth | Source_Infrared);
	SetUniqueName("Kinect " + std::string(serial));
}

KinectFreenectDevice::~KinectFreenectDevice()
{
	StopCapture(); //< Ensure thread has joined before closing the device
	freenect_close_device(m_device);
}

void KinectFreenectDevice::ThreadFunc(std::condition_variable& cv, std::mutex& m, std::exception_ptr& error)
{
	os_set_thread_name("KinectDeviceFreenect");

	// ── Video mode state ─────────────────────────────────────────────────
	// libfreenect supports one video mode at a time: either RGB or IR.
	// We track the current mode and switch when OBS requests a change.

	enum class VideoMode { RGB, IR };
	VideoMode currentVideoMode = VideoMode::RGB;

	freenect_frame_mode currentColorMode;
	currentColorMode.is_valid = 0;

	freenect_frame_mode currentIRMode;
	currentIRMode.is_valid = 0;

	freenect_frame_mode currentDepthMode;
	currentDepthMode.is_valid = 0;

	// ── Initial setup ────────────────────────────────────────────────────

	try
	{
		// RGB mode
		freenect_frame_mode colorMode = freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_RGB);
		if (!colorMode.is_valid)
			throw std::runtime_error("failed to find a valid RGB video mode");

		// IR 10-bit mode (uint16 per pixel, range 0-1023)
		freenect_frame_mode irMode = freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_IR_10BIT);
		if (!irMode.is_valid)
			throw std::runtime_error("failed to find a valid IR video mode");

		// Start with RGB
		if (freenect_set_video_mode(m_device, colorMode) < 0)
			throw std::runtime_error("failed to set RGB video mode");

		currentColorMode = colorMode;
		currentIRMode = irMode;

		// FREENECT_DEPTH_REGISTERED: depth in mm, aligned to 640x480 RGB
		freenect_frame_mode depthMode = freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_REGISTERED);
		if (!depthMode.is_valid)
			throw std::runtime_error("failed to find a valid depth mode");

		if (freenect_set_depth_mode(m_device, depthMode) < 0)
			throw std::runtime_error("failed to set depth mode");

		currentDepthMode = depthMode;
	}
	catch (const std::exception&)
	{
		error = std::current_exception();
	}

	{
		std::unique_lock<std::mutex> lk(m);
		cv.notify_all();
	} // m & cv no longer valid from here

	if (error)
		return;

	if (freenect_start_video(m_device) != 0)
		errorlog("failed to start video");

	if (freenect_start_depth(m_device) != 0)
		errorlog("failed to start depth");

	// ── Shared userdata for callbacks ────────────────────────────────────

	struct FreenectUserdata
	{
		std::mutex depthMutex;
		std::mutex videoMutex;
		std::uint32_t depthTimestamp = 0;
		std::uint32_t videoTimestamp = 0;
		std::vector<std::uint16_t> depthBackBuffer;
		std::vector<std::uint16_t> depthFrontBuffer;
		std::vector<std::uint8_t> videoBackBuffer;
		std::vector<std::uint8_t> videoFrontBuffer;
	};

	FreenectUserdata ud;
	ud.depthBackBuffer.resize(currentDepthMode.bytes);
	ud.depthFrontBuffer.resize(currentDepthMode.bytes);
	ud.videoBackBuffer.resize(currentColorMode.bytes); // RGB is larger than IR, safe default
	ud.videoFrontBuffer.resize(currentColorMode.bytes);

	freenect_set_user(m_device, &ud);

	freenect_set_depth_buffer(m_device, ud.depthBackBuffer.data());
	freenect_set_depth_callback(m_device, [](freenect_device* device, void* /*depth*/, uint32_t timestamp)
	{
		FreenectUserdata* userdata = static_cast<FreenectUserdata*>(freenect_get_user(device));
		std::scoped_lock lock(userdata->depthMutex);
		userdata->depthTimestamp = timestamp;
		std::swap(userdata->depthBackBuffer, userdata->depthFrontBuffer);
		freenect_set_depth_buffer(device, userdata->depthBackBuffer.data());
	});

	freenect_set_video_buffer(m_device, ud.videoBackBuffer.data());
	freenect_set_video_callback(m_device, [](freenect_device* device, void* /*video*/, uint32_t timestamp)
	{
		FreenectUserdata* userdata = static_cast<FreenectUserdata*>(freenect_get_user(device));
		std::scoped_lock lock(userdata->videoMutex);
		userdata->videoTimestamp = timestamp;
		std::swap(userdata->videoBackBuffer, userdata->videoFrontBuffer);
		freenect_set_video_buffer(device, userdata->videoBackBuffer.data());
	});

	// ── Main capture loop ────────────────────────────────────────────────

	SourceFlags enabledSourceFlags = Source_Color | Source_Depth | Source_ColorMappedDepth;

	while (IsRunning())
	{
		// ── Check for source flag updates from OBS ───────────────────
		if (auto sourceFlagUpdate = GetSourceFlagsUpdate())
		{
			SourceFlags newFlags = sourceFlagUpdate.value();
			bool wantIR = (newFlags & Source_Infrared) != 0;
			bool currentlyIR = (currentVideoMode == VideoMode::IR);

			if (wantIR != currentlyIR)
			{
				// Need to switch video mode
				freenect_stop_video(m_device);

				if (wantIR)
				{
					// Switch to IR mode
					if (freenect_set_video_mode(m_device, currentIRMode) == 0)
					{
						// Resize video buffers for IR (may be different size)
						{
							std::scoped_lock lock(ud.videoMutex);
							ud.videoBackBuffer.resize(currentIRMode.bytes);
							ud.videoFrontBuffer.resize(currentIRMode.bytes);
						}
						freenect_set_video_buffer(m_device, ud.videoBackBuffer.data());
						currentVideoMode = VideoMode::IR;
						infolog("switched to IR video mode");
					}
					else
					{
						errorlog("failed to switch to IR mode, staying in RGB");
						if (freenect_set_video_mode(m_device, currentColorMode) < 0)
							errorlog("failed to restore RGB mode");
					}
				}
				else
				{
					// Switch back to RGB mode
					if (freenect_set_video_mode(m_device, currentColorMode) == 0)
					{
						{
							std::scoped_lock lock(ud.videoMutex);
							ud.videoBackBuffer.resize(currentColorMode.bytes);
							ud.videoFrontBuffer.resize(currentColorMode.bytes);
						}
						freenect_set_video_buffer(m_device, ud.videoBackBuffer.data());
						currentVideoMode = VideoMode::RGB;
						infolog("switched to RGB video mode");
					}
					else
					{
						errorlog("failed to switch to RGB mode");
					}
				}

				freenect_start_video(m_device);
			}

			enabledSourceFlags = newFlags;
		}

		KinectFramePtr framePtr = std::make_shared<KinectFrame>();

		// ── Video frame ──────────────────────────────────────────────
		{
			std::scoped_lock lock(ud.videoMutex);

			if (currentVideoMode == VideoMode::RGB && (enabledSourceFlags & Source_Color))
			{
				const std::uint8_t* frameMem = ud.videoFrontBuffer.data();

				ColorFrameData& frameData = framePtr->colorFrame.emplace();
				frameData.width = currentColorMode.width;
				frameData.height = currentColorMode.height;

				// Convert RGB to RGBA
				std::size_t memSize = frameData.width * frameData.height * 4;
				frameData.memory.resize(memSize);
				std::uint8_t* memPtr = frameData.memory.data();

				for (std::size_t y = 0; y < frameData.height; ++y)
				{
					for (std::size_t x = 0; x < frameData.width; ++x)
					{
						*memPtr++ = frameMem[0];
						*memPtr++ = frameMem[1];
						*memPtr++ = frameMem[2];
						*memPtr++ = 0xFF;
						frameMem += 3;
					}
				}

				frameData.ptr.reset(frameData.memory.data());
				frameData.pitch = static_cast<std::uint32_t>(frameData.width * 4);
				frameData.format = GS_RGBA;
			}
			else if (currentVideoMode == VideoMode::IR && (enabledSourceFlags & Source_Infrared))
			{
				// IR 10-bit: each pixel is a uint16 (0-1023)
				// InfraredFrameData expects R16 (uint16 per pixel)
				const std::uint16_t* frameMem = reinterpret_cast<const std::uint16_t*>(ud.videoFrontBuffer.data());

				InfraredFrameData& frameData = framePtr->infraredFrame.emplace();
				frameData.width = currentIRMode.width;
				frameData.height = currentIRMode.height;

				std::size_t memSize = frameData.width * frameData.height * 2;
				frameData.memory.resize(memSize);

				// Scale 10-bit (0-1023) to 16-bit (0-65535) for better visibility
				std::uint16_t* memPtr = reinterpret_cast<std::uint16_t*>(frameData.memory.data());
				for (std::size_t i = 0; i < frameData.width * frameData.height; ++i)
					memPtr[i] = static_cast<std::uint16_t>(frameMem[i] << 6);

				frameData.ptr.reset(reinterpret_cast<std::uint16_t*>(frameData.memory.data()));
				frameData.pitch = static_cast<std::uint32_t>(frameData.width * 2);
			}
		}

		// ── Depth frame ──────────────────────────────────────────────
		// FREENECT_DEPTH_REGISTERED gives uint16 mm values, aligned to RGB
		{
			std::scoped_lock lock(ud.depthMutex);

			std::uint16_t* frameMem = ud.depthFrontBuffer.data();

			if (enabledSourceFlags & Source_Depth)
			{
				DepthFrameData& frameData = framePtr->depthFrame.emplace();
				frameData.width = currentDepthMode.width;
				frameData.height = currentDepthMode.height;

				std::size_t memSize = frameData.width * frameData.height * 2;
				frameData.memory.resize(memSize);
				std::memcpy(frameData.memory.data(), frameMem, memSize);

				frameData.ptr.reset(reinterpret_cast<std::uint16_t*>(frameData.memory.data()));
				frameData.pitch = static_cast<std::uint32_t>(frameData.width * 2);
			}

			// colorMappedDepth: same buffer, already registered/aligned to RGB
			if (enabledSourceFlags & Source_ColorMappedDepth)
			{
				DepthFrameData& frameData = framePtr->colorMappedDepthFrame.emplace();
				frameData.width = currentDepthMode.width;
				frameData.height = currentDepthMode.height;

				std::size_t memSize = frameData.width * frameData.height * 2;
				frameData.memory.resize(memSize);
				std::memcpy(frameData.memory.data(), frameMem, memSize);

				frameData.ptr.reset(reinterpret_cast<std::uint16_t*>(frameData.memory.data()));
				frameData.pitch = static_cast<std::uint32_t>(frameData.width * 2);
			}
		}

		UpdateFrame(std::move(framePtr));
		os_sleep_ms(1000 / 30);
	}

	if (freenect_stop_depth(m_device) != 0)
		errorlog("failed to stop depth");

	if (freenect_stop_video(m_device) != 0)
		errorlog("failed to stop video");

	infolog("exiting thread");
}
