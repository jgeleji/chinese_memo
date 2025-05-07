#include "tokenize.h"

std::vector<std::string> tokenize(std::string line, char delim)
{
	std::vector<std::string> ret;
	std::string token;
	for(size_t i=0; i<line.size(); ++i)
	{
		if(line[i] == delim)
		{
			ret.push_back(token);
			token.clear();
		}
		else
		{
			token += line[i];
		}
	}
	ret.push_back(token);
	return ret;
}
