#include "utils.h"

#include <cstdio>
#include <cstdlib>
#include <ctime>

std::mt19937& chinese_rng(void)
{
	static std::mt19937 gen = []()
	{
		// pinned seed? (only for test/replay; see utils.h)
		const char* env = getenv("CM_SEED");
		if(env && env[0])
			return std::mt19937((unsigned)strtoul(env, NULL, 10));
		const char* names[] = { "cm_seed", "files/cm_seed" };
		for(size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
		{
			FILE* f = fopen(names[i], "r");
			if(!f) continue;
			long v = -1;
			if(fscanf(f, "%ld", &v) == 1 && v >= 0)
			{
				fclose(f);
				return std::mt19937((unsigned)v);
			}
			fclose(f);
		}
		// normal case: a fresh seed every run (random_device, plus time and
		// ASLR in case a platform's random_device is not very random)
		std::random_device rd;
		unsigned seed = rd() ^ (unsigned)time(NULL) ^ (unsigned)(size_t)&gen;
		return std::mt19937(seed);
	}();
	return gen;
}

const char* reset_color       = "\033[0m";
const char* blue_background   = "\033[0;48;5;4m";
const char* red_background    = "\033[0;48;5;1m";
const char* green_foreground  = "\033[0;38;5;2m";
const char* yellow_foreground = "\033[0;38;5;3m";
const char* purple_background = "\033[0;48;5;5m";
const char* grey_background   = "\033[0;48;5;8m";
const char* green_background  = "\033[0;48;5;2m";
const char* yellow_background = "\033[0;48;5;3m";

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

