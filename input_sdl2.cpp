
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <random>
#include <SDL2/SDL.h>



#include "input.h"
#include "utils.h"


std::string chinese::Input::do_input_inner_raw(
	std::string& debug_string,
	std::string& raw_input,
	INPUT_STATE& state_number
) const
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT) {
			state_number = INPUT_STATE_EXIT;
			raw_input = "";
			return raw_input;
		}
		else if (event.type == SDL_KEYDOWN) {
			SDL_Keycode key = event.key.keysym.sym;
			if ((key >= SDLK_a && key <= SDLK_z) || (key >= SDLK_A && key <= SDLK_Z) || (key >= SDLK_0 && key <= SDLK_9)) {
				char ch = static_cast<char>(key);
				debug_string += std::string("char(") + ch + ")";
				raw_input += ch;
			}
			else if (key == SDLK_BACKSPACE) {
				debug_string += "code(BACKSPACE)";
				raw_input = raw_input.substr(0, std::max(0, ((int)raw_input.size()) - 1));
			}
			else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
				debug_string += "code(ENTER)";
				state_number = (INPUT_STATE)(((int)state_number) + 1);
			}
			else if (key == SDLK_ESCAPE) {
				debug_string += "code(ESCAPE)";
				state_number = INPUT_STATE_EXIT;
				raw_input = "";
			}
			else {
				debug_string += std::string("code(") + std::to_string((int)key) + ")";
			}
		}
	}

	if (raw_input == "exit") {
		state_number = INPUT_STATE_EXIT;
		raw_input = "";
	}

	return raw_input;
}

std::string chinese::Input::do_input_inner(
	std::string& debug_string,
	std::string& raw_input,
	INPUT_STATE& state_number
) const
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT) {
			state_number = INPUT_STATE_EXIT;
			raw_input = "";
			return raw_input;
		}
		else if (event.type == SDL_KEYDOWN) {
			SDL_Keycode key = event.key.keysym.sym;
			if ((key >= SDLK_a && key <= SDLK_z) || (key >= SDLK_A && key <= SDLK_Z) || (key >= SDLK_0 && key <= SDLK_9)) {
				char ch = static_cast<char>(key);
				debug_string += std::string("char(") + ch + ")";
				raw_input += ch;
			}
			else if (key == SDLK_BACKSPACE) {
				debug_string += "code(BACKSPACE)";
				raw_input = raw_input.substr(0, std::max(0, ((int)raw_input.size()) - 1));
			}
			else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
				debug_string += "code(ENTER)";
				state_number = (INPUT_STATE)(((int)state_number) + 1);
			}
			else if (key == SDLK_ESCAPE) {
				debug_string += "code(ESCAPE)";
				state_number = INPUT_STATE_EXIT;
				raw_input = "";
			}
			else {
				debug_string += std::string("code(") + std::to_string((int)key) + ")";
			}
		}
	}

	if (raw_input == "exit") {
		state_number = INPUT_STATE_EXIT;
		raw_input = "";
	}

	std::string pinyin = pinyin_convert_1syll(raw_input);
	return pinyin;
}


void chinese::Input::move_clear_refresh() const
{
	// SDL does not require explicit move, clear, refresh as in ncurses.
	// The rendering loop should handle screen updates.
}
