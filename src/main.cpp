#include <chrono>
#include <iostream>
#include <thread>

#include <Canvas.hpp>
#include <HttpRequest.hpp>
#include <kbRgb.hpp>
#include <main.hpp>
#include <Server.hpp>
#include <ServerWebService.hpp>
#include <Socket.hpp>
#include <string.hpp>
#include <Thread.hpp>

#include "input.hpp"

using namespace soup;

static bool running = true;
static bool force_update_input = false;
static input* active = nullptr;
static Canvas c{ 21, 6 };

static UniquePtr<input_chroma> in_chroma;
static UniquePtr<input_logitech> in_logitech;

static std::mutex outputs_mtx;
static std::vector<UniquePtr<kbRgb>> outputs;

static void pushCanvasToOutputs()
{
	std::lock_guard lock(outputs_mtx);
	for (auto& out : outputs)
	{
		if (out->name != "Razer Chroma" || !in_chroma || !in_chroma->isActive())
		{
			const uint8_t column_offset = out->getNumColumns() > 21 ? 1 : 0;
			Rgb keys[NUM_KEYS];
			for (uint8_t row = 0; row != 6; ++row)
			{
				for (uint8_t column = 0; column != 21; ++column)
				{
					if (auto sk = out->mapPosToKey(row, column + column_offset); sk != KEY_NONE)
					{
						keys[sk] = c.pixels[row * 21 + column];
					}
				}
			}
			out->setKeys(keys);
		}
	}
}

int entrypoint(std::vector<std::string>&& args, bool console)
{
	if (Socket::isPortLocallyBound(59084))
	{
		MessageBoxA(0, "Universal RGB is already running. Visit http://localhost:59084 in your browser manage it.", "Universal RGB", MB_OK);
		return 1;
	}

	Server serv;

	ServerWebService srv([](Socket& s, HttpRequest&& req, ServerWebService&)
	{
		if (req.path == "/api/effect")
		{
			if (active)
			{
				ServerWebService::sendHtml(s, c.toSvg(8));
			}
			else
			{
				ServerWebService::sendHtml(s, "");
			}
		}
		else if (req.path == "/")
		{
			ServerWebService::sendHtml(s,
#ifdef RELEASE
			#include "html/index.html.hpp"
#else
			string::fromFile("html/index.html")
#endif
			);
		}
		/*else if (req.path == "/api/effect-chroma")
		{
			if (in_chroma && in_chroma->isActive())
			{
				Canvas c(21, 6);
				in_chroma->getEffect(c.pixels.data());
				ServerWebService::sendHtml(s, c.toSvg(8));
			}
			else
			{
				ServerWebService::sendHtml(s, "");
			}
		}*/
		else if (req.path == "/api/outputs" || req.path == "/api/refresh-outputs" || req.path == "/api/refresh-outputs-nochroma")
		{
			if (req.path == "/api/refresh-outputs" || req.path == "/api/refresh-outputs-nochroma")
			{
				{
					std::lock_guard lock(outputs_mtx);
					for (auto& out : outputs)
					{
						out->deinit();
					}
					outputs = kbRgb::getAll(req.path == "/api/refresh-outputs");
				}
				Thread t([]
				{
					pushCanvasToOutputs();
				});
				t.awaitCompletion();
			}
			std::vector<std::string> output_names;
			{
				std::lock_guard lock(outputs_mtx);
				output_names.reserve(outputs.size());
				for (auto& out : outputs)
				{
					output_names.emplace_back(out->name);
				}
			}
			ServerWebService::sendText(s, string::join(output_names, "%"));
		}
		else if (req.path == "/api/inputs")
		{
			std::vector<std::string> input_names;
			if (in_chroma && in_chroma->enabled)
			{
				input_names.emplace_back("chroma");
			}
			if (in_logitech && in_logitech->enabled)
			{
				input_names.emplace_back("logitech");
			}
			ServerWebService::sendText(s, string::join(input_names, "%"));
		}
		else if (req.path == "/api/enable-input-chroma")
		{
			if (in_chroma)
			{
				in_chroma->enabled = true;	
				force_update_input = true;
				ServerWebService::sendText(s, "OK");
			}
			else
			{
				ServerWebService::sendText(s, "ERR");
			}
		}
		else if (req.path == "/api/disable-input-chroma")
		{
			if (in_chroma)
			{
				in_chroma->enabled = false;	
				force_update_input = true;
			}
			ServerWebService::sendText(s, "OK");
		}
		else if (req.path == "/api/enable-input-logitech")
		{
			if (in_logitech)
			{
				in_logitech->enabled = true;	
				force_update_input = true;
				ServerWebService::sendText(s, "OK");
			}
			else
			{
				ServerWebService::sendText(s, "ERR");
			}
		}
		else if (req.path == "/api/disable-input-logitech")
		{
			if (in_logitech)
			{
				in_logitech->enabled = false;	
				force_update_input = true;
			}
			ServerWebService::sendText(s, "OK");
		}
		else if (req.path == "/api/exit")
		{
			ServerWebService::sendText(s, "Universal RGB now exiting.");
			running = false;
			throw 0;
		}
		else
		{
			ServerWebService::send404(s);
		}
	});

	if (!serv.bind(59084, &srv)) // port decided by a fair dice roll :)
	{
		MessageBoxA(0, "Failed to bind TCP/59084.", "Universal RGB", MB_OK | MB_ICONERROR);
		return 2;
	}
	std::cout << "Listening on TCP/59084" << std::endl;

	try
	{
		in_chroma = soup::make_unique<input_chroma>();
	}
	catch (const std::exception& e)
	{
		std::cout << "Failed to initialise Chroma: " << e.what() << std::endl;
	}
	if (input_logitech::isAvailable())
	{
		in_logitech = soup::make_unique<input_logitech>();
	}
	else
	{
		std::cout << "Logitech Liaison not found\n";
	}

	outputs = kbRgb::getAll(false);

	Thread t([]
	{
		bool inited = false;
		while (running)
		{
			if (active)
			{
				if (active->isActive())
				{
					active->getEffect(c.pixels.data());
					inited = true;
					pushCanvasToOutputs();
					while (!active->hasUpdate() && running && !force_update_input)
					{
						active->awaitUpdate();
					}
					if (force_update_input)
					{
						force_update_input = false;
						active = nullptr;
					}
				}
				else
				{
					active = nullptr;
				}
			}
			else
			{
				if (in_chroma && in_chroma->enabled && in_chroma->isActive())
				{
					active = in_chroma.get();
					std::lock_guard lock(outputs_mtx);
					for (auto& out : outputs)
					{
						if (out->name == "Razer Chroma")
						{
							out->deinit();
						}
					}
				}
				else if (in_logitech && in_logitech->enabled && in_logitech->isActive())
				{
					active = in_logitech.get();
				}
				else
				{
					if (inited)
					{
						inited = false;
						for (auto& out : outputs)
						{
							out->deinit();
						}
					}
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}
			}
		}
	});

	try
	{
		serv.run();
	}
	catch (const int&)
	{
	}

	t.awaitCompletion();

	for (auto& out : outputs)
	{
		out->deinit();
	}

	return 0;
}

#ifdef RELEASE
// Using GUI type to avoid having a console window for a should-be background process.
SOUP_MAIN_GUI(entrypoint);
#else
SOUP_MAIN_CLI(entrypoint);
#endif
