#pragma once

#include <chrono>
#include <thread>

#include <IpAddr.hpp> // Need to include this to avoid Windows.h including a conflicting Winsock.h
#include <Rgb.hpp>

#include <Windows.h>

using namespace soup;

struct input
{
	bool enabled = true;

	virtual ~input() = default;

	virtual bool isActive() = 0;
	virtual void getEffect(Rgb out[6 * 21]) = 0;

	virtual bool hasUpdate() { return true; }
	virtual void awaitUpdate() { std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
};

struct input_chroma final : input
{
	//HANDLE h = INVALID_HANDLE_VALUE;
	void* view = nullptr;
	uint32_t read_index = -1;

	input_chroma();
	~input_chroma() final;

	bool isActive() final;
	void getEffect(Rgb out[6 * 21]) final;

	bool hasUpdate() final;
	void awaitUpdate() final;
};

struct input_logitech final : input
{
	[[nodiscard]] static bool isAvailable();
	[[nodiscard]] static std::string getRaw();

	bool isActive() final;
	void getEffect(Rgb out[6 * 21]) final;
};
