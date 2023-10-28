#include "input.hpp"

// Huge thanks to diogotr7 and his project https://github.com/diogotr7/RazerSdkReader

#include <structing.hpp>

#undef min

struct ChromaColour
{
	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;
	uint8_t a = 0;

	ChromaColour() = default;

	ChromaColour(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
		: r(r), g(g), b(b), a(a)
	{
	}

	[[nodiscard]] ChromaColour operator ^(const ChromaColour& other) const noexcept
	{
		return ChromaColour(r ^ other.r, g ^ other.g, b ^ other.b, a ^ other.a);
	}

	[[nodiscard]] bool operator==(const ChromaColour& other) const noexcept
	{
		return r == other.r && g == other.g && b == other.b && a == other.a;
	}

	[[nodiscard]] bool operator!=(const ChromaColour& other) const noexcept
	{
		return !operator==(other);
	}
};
static_assert(sizeof(ChromaColour) == 4);

struct RazerStaticEffect
{
	uint32_t dwParam;
	ChromaColour colour;
};
static_assert(sizeof(RazerStaticEffect) == 0x08);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-private-field"
struct ChromaKeyboardEffect
{
	struct Custom
	{
		ChromaColour colour[6 * 22];
	};
	static_assert(sizeof(Custom) == 528);

	struct Custom2
	{
		ChromaColour colour[6 * 22];
		ChromaColour key[6 * 22];
	};
	static_assert(sizeof(Custom2) == 1056);

	struct Custom3
	{
		ChromaColour colour[8 * 24];
		ChromaColour key[6 * 22];
	};
	static_assert(sizeof(Custom3) == 1296);

	PAD(0, 0x3C) RazerStaticEffect statik;
	/* 0x44 */ Custom custom;
	/* 0x254 */ Custom2 custom2;
	/* 0x674 */ Custom3 custom3;
};

enum class EffectType : uint32_t
{
	None = 0,
	Wave = 1,
	SpectrumCycling = 2,
	Breathing = 3,
	Blinking = 4,
	Reactive = 5,
	Static = 6,
	Custom = 7,
	CustomKey = 8,
	Init = 9,
	Uninit = 10,
	Default = 11,
	Starlight = 12,
	Suspend = 13,
	Resume = 14,
	Invalid = 15,
	Active = 16,
	Visualizer = 17
};

struct ChromaKeyboardData
{
	/* 0x00 */ uint32_t flag;
	/* 0x04 */ EffectType effect_type;
	/* 0x08 */ ChromaKeyboardEffect effect;
	PAD(0x08 + sizeof(ChromaKeyboardEffect), 0xB90) uint64_t timestamp;

	[[nodiscard]] ChromaColour getColour(uint32_t index) const noexcept
	{
		ChromaColour clr{};
		auto staticColour = effect.statik.colour;
		if (effect_type == EffectType::CustomKey)
		{
			clr = effect.custom2.key[index];

			if (clr == staticColour)
			{
				clr = effect.custom2.colour[index];
			}
		}
		else if (effect_type == EffectType::Custom
			|| effect_type == EffectType::Static
			|| effect_type == EffectType::Visualizer
			)
		{
			clr = effect.custom.colour[index];
		}
		return clr ^ staticColour;
	}
};
static_assert(sizeof(ChromaKeyboardData) == 0xB98);
#pragma clang diagnostic pop

struct ChromaKeyboard
{
	/* 0x00 */ uint32_t write_index;
	/* 0x04 */ uint32_t padding;
	/* 0x08 */ ChromaKeyboardData data[10];

	[[nodiscard]] uint32_t getReadIndex() const noexcept
	{
		return std::min(write_index - 1u, 9u);
	}
};

input_chroma::input_chroma()
{
	HANDLE h;

	h = OpenFileMappingA(FILE_MAP_READ, FALSE, R"(Global\{74164FAD-E73C-4FA1-A9AA-70813315ED9C})");
	if (h == NULL)
	{
		throw std::exception("Failed to open the file mapping");
	}
	view = MapViewOfFile(h, FILE_MAP_READ, 0, 0, sizeof(ChromaKeyboard));
	CloseHandle(h);
	if (!view)
	{
		throw std::exception("Failed to map a view of the file mapping");
	}

	//h = OpenEventA(SYNCHRONIZE, FALSE, R"(Global\{45C97C2C-2D50-4F30-B50E-AFBB1CE22E93})");
}

input_chroma::~input_chroma()
{
	/*if (h != NULL)
	{
		CloseHandle(h);
	}*/
	if (view)
	{
		UnmapViewOfFile(view);
	}
}

bool input_chroma::isActive()
{
	auto kbd = (ChromaKeyboard*)view;
	uint32_t read_index = kbd->getReadIndex();
	return kbd->data[read_index].effect_type != EffectType::None
		&& kbd->data[read_index].effect_type != EffectType::Default
		&& kbd->data[read_index].effect_type != EffectType::Suspend
		&& kbd->data[read_index].effect_type != EffectType::Active // this is used shortly before effect is active, but no data is available yet, apparently
		&& kbd->data[read_index].effect_type != EffectType::Resume
		;
}

void input_chroma::getEffect(Rgb out[6 * 21])
{
	auto kbd = (ChromaKeyboard*)view;

	read_index = kbd->getReadIndex();

	Rgb grid[6 * 22];
	for (uint32_t i = 0; i != 22 * 6; ++i)
	{
		ChromaColour clr = kbd->data[read_index].getColour(i);
		grid[i].r = clr.r;
		grid[i].g = clr.g;
		grid[i].b = clr.b;
	}

	for (uint8_t row = 0; row != 6; ++row)
	{
		for (uint8_t column = 0; column != 21; ++column)
		{
			out[row * 21 + column] = grid[row * 22 + column + 1];
		}
	}
}

bool input_chroma::hasUpdate()
{
	auto kbd = (ChromaKeyboard*)view;
	return kbd->getReadIndex();
}

void input_chroma::awaitUpdate()
{
	/*if (h != NULL)
	{
		WaitForSingleObject(h, INFINITE);
	}
	else*/
	{
		/*auto kbd = (ChromaKeyboard*)view;
		while (kbd->getReadIndex() == read_index)*/
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
}
