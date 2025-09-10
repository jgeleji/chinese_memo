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

std::string chinese::Input::do_input_pinyin(std::string description) const
{
	// 0 - inside single char flat pinyin
	// 1 - inside single char tone
	// 2 - finished char, decide to next char or finish input
	INPUT_STATE state_number = INPUT_STATE_TYPE_PINYIN;
	std::string ret;
	while(state_number != INPUT_STATE_EXIT)
	{
		move(0,0);
		clear();
		refresh();
		std::cout << description << "\r\n";
		std::cout << reset_color        << "0 " << ret << "\r\n";
		std::string add = this->do_input_1char_pinyin(ret, state_number, description);
		if(add == "-")
		{
			ret.clear();
			state_number = INPUT_STATE_TYPE_PINYIN;
		}
		else
		{
			ret += add;
		}
		//std::cout << "afterstuck " << state_number;
	}
	return ret;
}
std::string chinese::Input::do_input_1char_english(
	std::string const& top_row,
	INPUT_STATE& state_number,
	std::string const& description) const
{
	std::string debug_string, raw_input, chinese, inputted;
	do
	{
		inputted = this->do_input_inner_raw(debug_string, raw_input, state_number);
		static std::mt19937 rnd;
		move(0,0);
		clear();
		refresh();
		std::cout << description << "\r\n";
		std::cout << reset_color       << "0 " << top_row << "\r\n";
		std::cout << grey_background   << "1 " << raw_input << "\r\n";
		std::cout << red_background    << "2 " << inputted << reset_color << "\r\n";

	}
	while(state_number == INPUT_STATE_TYPE_PINYIN);
	return inputted;
}

std::string chinese::Input::do_input_1char_pinyin(
	std::string const& top_row,
	INPUT_STATE& state_number,
	std::string const& description) const
{
	std::string debug_string, raw_input, chinese, pinyin;
	do
	{
		pinyin = this->do_input_inner(debug_string, raw_input, state_number);
		move(0,0);
		clear();
		refresh();
		std::cout << description << "\r\n";
		std::cout << reset_color       << "0 " << top_row << "\r\n";
		std::cout << purple_background   << "1 " << raw_input << "\r\n";
		std::cout << red_background    << "2 " << pinyin << reset_color << "\r\n";

	}
	while(state_number == INPUT_STATE_TYPE_PINYIN);
	//std::cout << "Stuck ";
	if(pinyin.empty() || raw_input.empty())
	{
		state_number = INPUT_STATE_EXIT;
		//std::cout << "EXIT";
	}
	else
	{
		state_number = INPUT_STATE_TYPE_PINYIN;
		//std::cout << "PINYIN";
	}
	//std::cout << " " << pinyin;
	return pinyin;
}

std::string chinese::Input::do_input_english(std::string description) const
{
	// 0 - inside single char flat pinyin
	// 1 - inside single char tone
	// 2 - finished char, decide to next char or finish input
	INPUT_STATE state_number = INPUT_STATE_TYPE_PINYIN;
	std::string ret;
	while(state_number != INPUT_STATE_EXIT)
	{
		move(0,0);
		clear();
		refresh();
		std::cout << description << "\r\n";
		std::cout << reset_color        << "0 " << ret << "\r\n";
		std::string c = this->do_input_1char_english(ret, state_number, description);
		if(c=="-")
		{
			ret.clear();
		}
		else
		{
			ret += c;
		}
	}
	return ret;
}

std::string chinese::Input::do_input_chinese(std::string description, bool shuffle, int breaks, const char* expected) const
{
	// 0 - inside single char flat pinyin
	// 1 - inside single char tone
	// 2 - finished char, decide to next char or finish input
	INPUT_STATE state_number = INPUT_STATE_TYPE_PINYIN;
	std::string ret;
	while(state_number != INPUT_STATE_EXIT)
	{
		move(0,0);
		clear();
		refresh();
		std::cout << description << "\r\n";
		std::cout << reset_color        << "0 " << ret << "\r\n";
		std::string add = this->do_input_1char_chinese(
			ret,
			state_number,
			description,
			shuffle,
			shuffle,
			expected,
			breaks
		);
		if(add=="-")
		{
			ret.clear();
		}
		else
		{
			ret += add;
		}
	}
	return ret;
}


std::string chinese::Input::do_input_1char_chinese(
	std::string const& top_row,
	INPUT_STATE& state_number,
	std::string const& description,
	bool shuffle,
	bool show_others,
	const char* expected,
	int breaks) const
{
	std::string debug_string, raw_input, chinese, pinyin;
	std::vector<std::string> expected_chinese_chars_vec;
	if(expected)
	{
		expected_chinese_chars_vec = split_into_chinese_characters(expected);
	}
	std::unordered_set<std::string> expected_chinese_chars(expected_chinese_chars_vec.begin(), expected_chinese_chars_vec.end());
	std::vector<std::string> ch_ch_vec;
	do
	{
		pinyin = this->do_input_inner(debug_string, raw_input, state_number);
		ch_ch_vec = this->get_possibles(pinyin);
		std::unordered_set<std::string> ch_ch(ch_ch_vec.begin(), ch_ch_vec.end());
		static std::mt19937 rnd;
		if(show_others)
		{
			std::vector<std::string> tmp = ch_ch_vec;
			ch_ch_vec.clear();
			for(std::string solution: tmp)
			{
				if(expected_chinese_chars.find(solution) != expected_chinese_chars.end())
				{
					ch_ch_vec.push_back(solution);
				}
				//MICSODA
			}
			tmp = get_some_random_chinese_chars(10 - ch_ch_vec.size(), rnd);
			for(std::string random_char:tmp)
			{
				ch_ch_vec.push_back(random_char);
			}
			std::shuffle(ch_ch_vec.begin(), ch_ch_vec.end(), rnd);
		}
		else if(shuffle)
		{
			std::shuffle(ch_ch_vec.begin(), ch_ch_vec.end(), rnd);
		}
		std::vector<std::stringstream> chinese_choices;
		chinese_choices.push_back(std::stringstream());
		for(size_t i=0; i<ch_ch_vec.size(); ++i)
		{
			chinese_choices.back() << pad_to_three(i+1) << " " << ch_ch_vec[i] << " ";
			if(breaks>0 && i%breaks==breaks-1)
			{
				chinese_choices.push_back(std::stringstream());
			}
		}
		move(0,0);
		clear();
		refresh();
		std::cout << description << "\r\n";
		std::cout << reset_color       << "0 " << top_row << "\r\n";
		std::cout << blue_background   << "1 " << raw_input << "\r\n";
		std::cout << red_background    << "2 " << pinyin << reset_color << "\r\n";
		if(!shuffle)
		{
			std::cout << yellow_foreground;
			for(size_t i=0; i<chinese_choices.size(); ++i)
			{
				std::cout << chinese_choices[i].str() << "\r\n";
			}
		}

	}
	while(state_number == INPUT_STATE_TYPE_PINYIN);
	std::vector<std::stringstream> chinese_choices;
	chinese_choices.push_back(std::stringstream());
	for(size_t i=0; i<ch_ch_vec.size(); ++i)
	{
		chinese_choices.back() << pad_to_three(i+1) << " " << ch_ch_vec[i] << " ";
		if(breaks>0 && i%breaks==breaks-1)
		{
			chinese_choices.push_back(std::stringstream());
		}
	}
	raw_input.clear();
	move(0,0);
	clear();
	refresh();
	std::cout << reset_color        << "0 " << top_row << "\r\n";
	std::cout << grey_background    << "1 " << "\r\n";
	std::cout << red_background     << "2 " << pinyin << "\r\n";
	std::cout << yellow_foreground;
	for(size_t i=0; i<chinese_choices.size(); ++i)
	{
		std::cout << chinese_choices[i].str() << "\r\n";
	}
	std::cout << green_foreground   << "4 " << raw_input << "\r\n";
	while(state_number == INPUT_STATE_CHOOSE_NUMBER)
	{
		this->do_input_inner(debug_string, raw_input, state_number);
		move(0,0);
		clear();
		refresh();
		std::cout << reset_color        << "0 " << top_row << "\r\n";
		std::cout << grey_background    << "1 " << "\r\n";
		std::cout << red_background     << "2 " << pinyin << "\r\n";
		std::cout << yellow_foreground;
		for(size_t i=0; i<chinese_choices.size(); ++i)
		{
			std::cout << chinese_choices[i].str() << "\r\n";
		}
		std::cout << green_foreground   << "4 " << raw_input << "\r\n";
	}
	if(raw_input.empty())
	{
		state_number = INPUT_STATE_EXIT;
		return pinyin;
	}
	int number = std::atoi(raw_input.c_str());
	if(number == 0 || ch_ch_vec.empty())
	{
		state_number = INPUT_STATE_TYPE_PINYIN;
		return pinyin;
	}
	state_number = INPUT_STATE_TYPE_PINYIN;
	return ch_ch_vec[(number-1) % ch_ch_vec.size()];
}

