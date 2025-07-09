#include <fstream>
#include <iostream>
#include <string>

#include "input.h"
#include "tokenize.h"

struct datapoint{
	std::string chinese;
	std::string english;
	std::string pinyin;
	std::string auto_pinyin;
};

int main(int argc, const char* argv[])
{
	chinese::Input input;

	for(int i=1; i<argc; ++i)
	{
		std::cout << "Converting " << argv[i] << "\n";
		std::fstream infile;
		infile.open(argv[i], std::fstream::in);
		std::vector<datapoint> data;
		while(infile.good())
		{
			std::string line;
			getline(infile, line);
			if(line.empty()) continue;
			if(line[0]==0) continue;
			std::vector<std::string> tokens = tokenize(line, '|');
			if(tokens.size() < 2) continue;
			datapoint curr;
			curr.chinese = tokens[0];
			curr.english = tokens[1];
			if(tokens.size() > 2)
			{
				curr.pinyin = tokens[2];
				curr.auto_pinyin = "";
			}
			else
			{
				curr.pinyin = "";
				curr.auto_pinyin = input.convert_chinese_to_pinyin(curr.chinese);
			}
			data.push_back(curr);
		}
		std::string outfilename = std::string("autoex_") + argv[i];
		std::cout << "Writing " << outfilename << "\n";
		std::fstream outfile;
		outfile.open(outfilename, std::fstream::out);
		for(int i=0; i<data.size(); ++i)
		{
			datapoint const& dp = data[i];
			outfile << dp.chinese << "|" << dp.english << "|" << dp.pinyin << "|" << dp.auto_pinyin << "\n";
		}
		std::cout << "Done.\n";
	}
	return 0;
}
