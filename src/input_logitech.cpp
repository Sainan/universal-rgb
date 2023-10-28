#include "input.hpp"

#include <HttpRequest.hpp>
#include <Socket.hpp>
#include <StringReader.hpp>
#include <Uri.hpp>

bool input_logitech::isAvailable()
{
	return Socket::isPortLocallyBound(49748);
}

std::string input_logitech::getRaw()
{
	if (auto resp = HttpRequest(Uri("http://localhost:49748/raw")).execute())
	{
		return resp->body;
	}
	return {};
}

bool input_logitech::isActive()
{
	return !getRaw().empty();
}

void input_logitech::getEffect(Rgb out[6 * 21])
{
	StringReader sr(getRaw());
	for (uint8_t i = 0; i != 6 * 21; ++i)
	{
		sr.u8(out[i].r);
		sr.u8(out[i].g);
		sr.u8(out[i].b);
	}

	// Logitech's bitmap does not seem to have an empty space between ESC and F1.
	out[13] = out[12];
	out[12] = out[11];
	out[11] = out[10];
	out[10] = out[9];
	out[9] = out[8];
	out[8] = out[7];
	out[7] = out[6];
	out[6] = out[5];
	out[5] = out[4];
	out[4] = out[3];
	out[3] = out[2];
	out[2] = out[1];
	out[1] = Rgb::BLACK;
}
