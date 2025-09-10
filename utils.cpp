#include "utils.h"

const char* reset_color       = "\033[0m";
const char* blue_background   = "\033[0;48;5;4m";
const char* red_background    = "\033[0;48;5;1m";
const char* green_foreground  = "\033[0;38;5;2m";
const char* yellow_foreground = "\033[0;38;5;3m";
const char* purple_background = "\033[0;48;5;5m";
const char* grey_background   = "\033[0;48;5;8m";

std::string pad_to_three(int i)
{
	if(0<=i && i<10)
	{
		return " " + std::to_string(i) + ".";
	}
	if(10 <= i && i < 100)
	{
		return std::to_string(i) + ".";
	}
	return std::to_string(i);
}

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

