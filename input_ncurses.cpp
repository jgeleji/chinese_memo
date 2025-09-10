#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <random>
#include <ncurses.h>
#include <curses.h>

#include "input.h"
#include "utils.h"

std::string chinese::Input::do_input_inner_raw(
	std::string& debug_string,
	std::string& raw_input,
	INPUT_STATE& state_number
) const
{
	char ch = getch();
	if('a' <= ch && ch <= 'z' || 'A' <= ch && ch <= 'Z' || '0' <= ch && ch <= '9')
	{
		debug_string += std::string("char(") + ch + ")";
	}
	else
	{
		debug_string += std::string("code(") + std::to_string((int)ch) + ")";
	}
	if(((int)ch) == 7 || ((int)ch) == 127)
	{
		raw_input = raw_input.substr(0, std::max(0, ((int)raw_input.size())-1));
	}
	else if(((int)ch) == 10)
	{
		state_number = (INPUT_STATE)(((int)state_number)+1);
	}
	else
	{
		raw_input += ch;
	}
	if(raw_input=="exit")
	{
		state_number = INPUT_STATE_EXIT;
		raw_input="";
	}

	return raw_input;
}

std::string chinese::Input::do_input_inner(
	std::string& debug_string,
	std::string& raw_input,
	INPUT_STATE& state_number
) const
{
	char ch = getch();
	if('a' <= ch && ch <= 'z' || 'A' <= ch && ch <= 'Z' || '0' <= ch && ch <= '9')
	{
		debug_string += std::string("char(") + ch + ")";
	}
	else
	{
		debug_string += std::string("code(") + std::to_string((int)ch) + ")";
	}
	if(((int)ch) == 7 || ((int)ch) == 127)
	{
		raw_input = raw_input.substr(0, std::max(0, ((int)raw_input.size())-1));
	}
	else if(((int)ch) == 10)
	{
		state_number = (INPUT_STATE)(((int)state_number)+1);
	}
	else
	{
		raw_input += ch;
	}
	if(raw_input=="exit")
	{
		state_number = INPUT_STATE_EXIT;
		raw_input="";
	}

	std::string pinyin = pinyin_convert_1syll(raw_input);
	return pinyin;
}


void chinese::Input::move_clear_refresh() const
{
	move(0,0);
	clear();
	refresh();
}

void chinese::Input::init() const
{
	initscr();			/* Start curses mode 		*/
	//raw();				/* Line buffering disabled	*/
	keypad(stdscr, TRUE);		/* We get F1, F2 etc..		*/
	noecho();			/* Don't echo() while we do getch */
}

void chinese::Input::close() const
{
	endwin();			/* End curses mode		  */
}

void chinese::Input::system_pause() const
{
	getch();
}

int chinese::Input::cols() const
{
	return COLS;
}
