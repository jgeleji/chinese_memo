#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <random>
#include <ncurses.h>
#include <curses.h>

#include "input.h"

static const bool debugprints = false;

const char* reset_color       = "\033[0m";
const char* blue_background   = "\033[0;48;5;4m";
const char* red_background    = "\033[0;48;5;1m";
const char* green_foreground  = "\033[0;38;5;2m";
const char* yellow_foreground = "\033[0;38;5;3m";
const char* purple_foreground = "\033[0;38;5;5m";
const char* grey_background   = "\033[0;48;5;8m";

bool replaceall(std::string& in, std::string from, std::string to)
{
	size_t pos = in.find(from);
	if(pos == std::string::npos) return false;
	std::string first_half = in.substr(0, pos);
	std::string second_half = in.substr(pos + from.size());
	if(second_half.size()>0)
	{
		replaceall(second_half, from, to);
	}
	in = first_half + to + second_half;
	return true;
}

std::vector<std::string> chinese::Input::get_possibles(std::string const& pinyin) const
{
	std::vector<std::string> ret;
	auto iter0 = pinyin_to_chinese.find(pinyin);
	if(iter0 != pinyin_to_chinese.end())
	{
		for(auto iter1 = iter0->second.begin(); iter1 != iter0->second.end(); ++iter1)
		{
			ret.push_back(*iter1);
		}
	}
	class sorter
	{
			std::unordered_map<std::string, int> const* chinese_to_frequency;
		public:
			sorter(std::unordered_map<std::string, int> const* val) : chinese_to_frequency(val)
			{
			}
			
			bool operator()(std::string const& lhs, std::string const& rhs) const
			{
				int lhs_val = std::numeric_limits<int>::max();
				int rhs_val = std::numeric_limits<int>::max();
				auto iter_lhs = chinese_to_frequency->find(lhs);
				if(iter_lhs != chinese_to_frequency->end())
				{
					lhs_val = iter_lhs->second;
				}
				auto iter_rhs = chinese_to_frequency->find(rhs);
				if(iter_rhs != chinese_to_frequency->end())
				{
					rhs_val = iter_rhs->second;
				}
				return lhs_val < rhs_val;
			}
	};
	std::sort(ret.begin(), ret.end(), sorter(&this->chinese_to_frequency));
	return ret;
}

std::vector<std::string> chinese::Input::get_possibles_fromflat(std::string const& pinyin) const
{
	std::vector<std::string> ret;
	int tone;
	std::string flat_pinyin = pinyin_flatten_1syll(pinyin, tone);
	static const std::string tones[] = {"-", "/", "ˇ", "\\", ""};
	for(int i=0; i<sizeof(tones)/sizeof(tones[0]); ++i)
	{
		std::string tonechar = tones[i];
		std::vector<std::string> part = get_possibles(pinyin + tonechar);
		ret.insert(ret.end(), part.begin(), part.end());
	}
	return ret;
}

chinese::Input::Input()
{
	std::string infile_name = "data/general-standard?page=";

	size_t failcount = 0;
	size_t charnum=0;
	for(int i=1; i<=82; ++i)
	{
		std::fstream infile;
		infile.open(infile_name+std::to_string(i), std::fstream::in);
		size_t line_no = 0;
		while(infile.good())
		{
			try {
				std::string line;
				getline(infile, line);
				++line_no;
				if(line.size() < 10 || line.substr(0, 10) != "<tr><td><a") continue;
				//<tr><td><a href="/character/一">一</a></td><td><span style="color:#990000;">yī</span></td><td><span class="smmr">one; a, an; alone</span></td><td><a href="/character/一" title="Kangxi radical 1">一</a>&nbsp;1.0</td><td>1</td><td>1</td><td>0001</td></td><td>2</td></tr>
				line = line.substr(28);
				size_t pos = line.find("\"");
				std::string chinese = line.substr(0, pos);
				pos = line.find("<td>");
				line = line.substr(pos+4);
				pos = line.find("</td>");
				std::string pinyin_td = line.substr(0, pos);
				std::string pinyin;
				if(pinyin_td.size() > 10)
				{
					pinyin_td = pinyin_td.substr(29);
					pinyin = pinyin_td.substr(0, pinyin_td.size()-7);
				}
				else
				{
					pinyin = pinyin_td;
				}
				if(pinyin.size()==0)
				{
					pinyin= "NA";
				}
				++charnum;
				pos = line.rfind("<td>");
				int rank=-1;
				if(pos != std::string::npos)
				{
					std::string rankstr = line.substr(pos+4).c_str();
					rankstr = rankstr.substr(0, rankstr.size()-10);
					rank = atoi(rankstr.c_str());
				}
				chinese_to_frequency[chinese] = rank;


				chinese_to_pinyin[chinese].insert(pinyin);
				chinese_to_preferred_pinyin[chinese] = pinyin;

				pinyin_to_chinese[pinyin].insert(chinese);
				
				if(debugprints)
				{
					std::cout << charnum << ". pinyin = '" << pinyin << "' chinese = '";
					std::cout << chinese << " freq = " << rank << "\n";
				}
				std::string converted = pinyin_convert_1syll(pinyin);
				if(converted != pinyin)
				{
					//std::cout << "pinyin = '" << pinyin << "' chinese = '" << chinese << " ";
					std::cout << "Fail! converted='" << converted << "'\n";
					++failcount;
				}

			}
			catch(std::exception const& e)
			{
				std::cout << "Error parsing line " << line_no << " in file " << (infile_name+std::to_string(i)) << "\n";
			}
		}
	}
	for(auto iter_chinese_char = chinese_to_pinyin.begin(); iter_chinese_char != chinese_to_pinyin.end(); ++iter_chinese_char)
	{
		try
		{
			std::string chinese_char = iter_chinese_char->first;
			std::fstream infile;
			infile.open("data2/"+chinese_char, std::fstream::in);
			while(infile.good())
			{
				std::string line;
				getline(infile, line);
				if(line.size() <= 4 || line.substr(0,4) != "<h1>")
				{
					continue;
				}
				size_t span_start = line.find("<span");
				while(span_start != std::string::npos)
				{
					line = line.substr(span_start);
					size_t pos = line.find(">");
					line = line.substr(pos+1);
					pos = line.find("<");
					std::string pinyin = line.substr(0, pos);
					if(pinyin.front() != '(' && pinyin.front() != '/')
					{
						pinyin_to_chinese[pinyin].insert(chinese_char);
						chinese_to_pinyin[chinese_char].insert(pinyin);
					}
					line = line.substr(pos+1);
					span_start = line.find("<span");
				}
			}
		}
		catch(std::exception const& e)
		{
		}
	}
	if(debugprints)
	{
		std::cout << "failcount = " << failcount << "\n";
	}
	chinese_to_preferred_pinyin["家"] = pinyin_convert_1syll("jia1");
	chinese_to_preferred_pinyin["吗"] = pinyin_convert_1syll("ma");
	chinese_to_preferred_pinyin["漂"] = pinyin_convert_1syll("piao4");
	chinese_to_preferred_pinyin["亮"] = pinyin_convert_1syll("liang");
	chinese_to_preferred_pinyin["儿"] = pinyin_convert_1syll("er");
	chinese_to_preferred_pinyin["哪"] = pinyin_convert_1syll("na3");
	chinese_to_preferred_pinyin["不"] = pinyin_convert_1syll("bu4");
	chinese_to_preferred_pinyin["乐"] = pinyin_convert_1syll("yue4");


	chinese_to_preferred_pinyin["觉"] = pinyin_convert_1syll("jiao4");

	for(auto iter = chinese_to_preferred_pinyin.begin(); iter != chinese_to_preferred_pinyin.end(); ++iter)
	{
		std::string chinese = iter->first;
		std::string pinyin = iter->second;
		chinese_to_pinyin[chinese].insert(pinyin);
		pinyin_to_chinese[pinyin].insert(chinese);
	}
	//chinese_to_preferred_pinyin[] = "";
}

std::unordered_set<std::string> const& chinese::Input::get_chinese(std::string const& pinyin)
{
	return pinyin_to_chinese[pinyin_convert(pinyin)];
}

std::string chinese::Input::pinyin_convert(std::string const& raw_input)
{
	static const std::vector<std::string> initials =
	{
		"b", "p", "m", "f",
		"d", "t", "n", "l",
		"g", "k", "h",
		"j", "q", "x",
		"z", "c", "s",
		"zh", "ch", "sh", "r"
	};
	static const std::vector<std::string> endings =
	{
		"a", "ai", "ao", "an", "ang",
		"e", "ei", "en", "eng",
		"o", "ou", "ong",
		"i", "ia", "ie", "iao", "iu",
		"ian", "in",
		"iang", "ing", "iong",
		"u", "ua", "uo", "uai", "ui",
		"uan", "un", "uang", "ueng",
		"ü", "üe", "üan", "ün",
		"er"
	};
	return "";
}

std::string chinese::Input::pinyin_flatten_1syll(std::string const& raw_input, int& tone)
{
	//flatten start
	std::string pinyin = raw_input;
	bool changed_something = false;
	do {
		changed_something = false;
		for(int i=0; i<replace.size(); ++i)
		{
			changed_something = changed_something || replaceall(pinyin, replace[i].second, replace[i].first);
		}
	} while(changed_something);
	tone=0;
	for(int i=1; i<=4; ++i)
	{
		if(replaceall(pinyin, std::to_string(i), ""))
		{
			tone = i;
			break;
		}
	}
	return pinyin;
	//flatten finished, extracted tone information
}

const std::vector<std::pair<std::string, std::string>> chinese::Input::replace = {

	{"1", "-"},
	{"2", "/"},
	{"3", "ˇ"},
	{"4", "\\"},

	{"a-", "ā"},
	{"a/", "á"},
	{"aˇ", "ǎ"},
	{"a\\", "à"},

	{"i-", "ī"},
	{"i/", "í"},
	{"iˇ", "ǐ"},
	{"i\\", "ì"},

	{"e-", "ē"},
	{"e/", "é"},
	{"eˇ", "ě"},
	{"e\\", "è"},

	{"u-", "ū"},
	{"u/", "ú"},
	{"uˇ", "ǔ"},
	{"u\\", "ù"},

	{"o-", "ō"},
	{"o/", "ó"},
	{"oˇ", "ǒ"},
	{"o\\", "ò"},

	{"ü-", "ǖ"},
	{"ü/", "ǘ"},
	{"üˇ", "ǚ"},
	{"ü\\", "ǜ"}
};

std::string chinese::Input::pinyin_convert_1syll(std::string const& raw_input)
{
	int tone;
	std::string pinyin = pinyin_flatten_1syll(raw_input, tone);

	//find any vowel
	static const std::vector<std::string> vowels = {"a", "o", "e", "i", "u", "ü"};
	bool found_vowel = false;
	for(size_t i=0; i<vowels.size(); ++i)
	{
		if(pinyin.find(vowels[i]) != std::string::npos)
		{
			found_vowel = true;
			break;
		}
	}
	//if(!found_vowel)
	//{
	//	std::cout << "Vowel not found! \"" << raw_input << "\"\n";
	//}
	//find any vowel finished

	//find strongest vowel (if have tone)
	static std::vector<char> tone_attraction_order = {'a', 'o', 'e'};
	bool found_attractor=false;
	if(tone > 0)
	{
		for(size_t i=0; i < tone_attraction_order.size(); ++i)
		{
			size_t pos = pinyin.find(tone_attraction_order[i]);
			if(pos == std::string::npos)
			{
				continue;
			}
			replaceall(pinyin, std::string(1, tone_attraction_order[i]), std::string(1, tone_attraction_order[i]) + std::to_string(tone));
			found_attractor=true;
			break;
		}
		//otherwise use last vowel
		if(!found_attractor)
		{
			size_t pos = pinyin.find_last_of("iuü")+1;
			pinyin = pinyin.substr(0, pos) + std::to_string(tone) + pinyin.substr(pos);
		}
	}
	//find strongest vowel finished

	//decorate
	bool changed_something = false;
	do {
		changed_something = false;
		for(int i=0; i<replace.size(); ++i)
		{
			changed_something = changed_something || replaceall(pinyin, replace[i].first, replace[i].second);
		}
	} while(changed_something);
	//decorate finished
	return pinyin;
}

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
		std::cout << blue_background   << "1 " << raw_input << "\r\n";
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
		std::cout << blue_background   << "1 " << raw_input << "\r\n";
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
		ret += this->do_input_1char_english(ret, state_number, description);
	}
	return ret;
}

std::string chinese::Input::do_input_chinese(std::string description) const
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
		std::string add = this->do_input_1char_chinese(ret, state_number, description);
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
	std::string const& description) const
{
	std::string debug_string, raw_input, chinese, pinyin;
	std::vector<std::string> ch_ch_vec;
	do
	{
		pinyin = this->do_input_inner(debug_string, raw_input, state_number);
		ch_ch_vec = this->get_possibles(pinyin);
		static std::mt19937 rnd;
		std::shuffle(ch_ch_vec.begin(), ch_ch_vec.end(), rnd);
		std::stringstream chinese_choices;
		for(size_t i=0; i<ch_ch_vec.size(); ++i)
		{
			chinese_choices << (i+1) << ". " << ch_ch_vec[i] << "  ";
		}
		move(0,0);
		clear();
		refresh();
		std::cout << description << "\r\n";
		std::cout << reset_color       << "0 " << top_row << "\r\n";
		std::cout << blue_background   << "1 " << raw_input << "\r\n";
		std::cout << red_background    << "2 " << pinyin << reset_color << "\r\n";
		//std::cout << yellow_foreground << "3 " << chinese_choices.str() << std::flush;

	}
	while(state_number == INPUT_STATE_TYPE_PINYIN);
	std::stringstream chinese_choices;
	for(size_t i=0; i<ch_ch_vec.size(); ++i)
	{
		chinese_choices << (i+1) << ". " << ch_ch_vec[i] << "  ";
	}
	raw_input.clear();
	move(0,0);
	clear();
	refresh();
	std::cout << reset_color        << "0 " << top_row << "\r\n";
	std::cout << grey_background    << "1 " << "\r\n";
	std::cout << red_background     << "2 " << pinyin << "\r\n";
	std::cout << yellow_foreground  << "3 " << chinese_choices.str() << "\r\n";
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
		std::cout << yellow_foreground  << "3 " << chinese_choices.str() << "\r\n";
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

std::string chinese::Input::convert_chinese_to_pinyin(std::string chinese) const
{
	std::string ret;
	size_t pos = 0;
	for(size_t i = 0; i < chinese.size(); ++i)
	{
		char c = chinese[i];
		if(0 == (c & 0x80)) // single byte case
		{
			ret += c;
			//single byte cannot be chinese character. don't even try converting.
		}
		else if((c & 0xC0) == 0xC0 && (c & 0x20) == 0) // 2 byte case
		{
			std::string utf8_char;
			utf8_char += c;
			++i;
			c = chinese[i];
			utf8_char += c;
			auto iter0 = chinese_to_preferred_pinyin.find(utf8_char);
			if(iter0 == chinese_to_preferred_pinyin.end())
			{
				auto iter = chinese_to_pinyin.find(utf8_char);
				ret += *iter->second.begin();
			}
			else
			{
				ret += iter0->second;
			}
		}
		else if((c & 0xE0) == 0xE0 && (c & 0x10) == 0) // 3 byte case
		{
			std::string utf8_char;
			utf8_char += c;
			++i;
			c = chinese[i];
			utf8_char += c;
			++i;
			c = chinese[i];
			utf8_char += c;
			auto iter0 = chinese_to_preferred_pinyin.find(utf8_char);
			if(iter0 == chinese_to_preferred_pinyin.end())
			{
				auto iter = chinese_to_pinyin.find(utf8_char);
				ret += *iter->second.begin();
			}
			else
			{
				ret += iter0->second;
			}
		}
		else if((c & 0xF0) == 0xF0 && (c & 0x08) == 0) // 4 byte case
		{
			std::string utf8_char;
			utf8_char += c;
			++i;
			c = chinese[i];
			utf8_char += c;
			++i;
			c = chinese[i];
			utf8_char += c;
			++i;
			c = chinese[i];
			utf8_char += c;
			auto iter0 = chinese_to_preferred_pinyin.find(utf8_char);
			if(iter0 == chinese_to_preferred_pinyin.end())
			{
				auto iter = chinese_to_pinyin.find(utf8_char);
				ret += *iter->second.begin();
			}
			else
			{
				ret += iter0->second;
			}
		}
	}
	return ret;
}


