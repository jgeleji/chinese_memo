#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <random>
#include <chrono>
#include <thread>

#include <ncurses.h>
#include <curses.h>

#include "questions.h"
#include "tokenize.h"

#define PRINT(X) std::cout << __FILE__ << ":" << __LINE__ << " " << (#X) << " = " << (X) << std::endl

questions::questions()
{
}

void questions::load_file(std::string const& filename)
{
	std::fstream infile;
	infile.open(filename, std::fstream::in);
	size_t line_no = 0;
	while(infile.good())
	{
		std::string line;
		getline(infile, line);
		++line_no;
		//PRINT(line_no);
		std::vector<std::string> tokens = tokenize(line, '|');
		if(tokens.size() < 2) continue;
		datapoint dp;
		dp.chinese = tokens[0];
		dp.english = tokens[1];
		if(tokens.size()>2)
		{
			dp.pinyin = tokens[2];
		}
		else
		{
			dp.pinyin = m_input.convert_chinese_to_pinyin(dp.chinese);
		}
		loaded_data.push_back(dp);
		pinyin_overlaps[dp.pinyin].insert(loaded_data.size()-1);
	}
	//PRINT(loaded_data.size());
}

bool questions::ask_1(
	size_t which,
	DATATYPE provided,
	DATATYPE asked
) const
{
	std::vector<datapoint const*> others;
	return loaded_data[which].ask(m_input, provided, asked, std::string(), others);
}

std::string questions::to_string(DATATYPE val)
{
	switch(val)
	{
		case DATATYPE_CHINESE:
			return "chinese";
		case DATATYPE_PINYIN:
			return "pinyin";
		case DATATYPE_ENGLISH:
			return "english";
	}
	return "unknown";
}

std::string const& questions::datapoint::get(DATATYPE which) const
{
	static const std::string undefined = "undefined";
	switch(which)
	{
		case DATATYPE_CHINESE:
			return chinese;
		case DATATYPE_PINYIN:
			return pinyin;
		case DATATYPE_ENGLISH:
			return english;
	}
	return undefined;
}

bool questions::datapoint::ask(
	chinese::Input input,
	questions::DATATYPE provided,
	questions::DATATYPE asked,
	std::string const& question_number,
	std::vector<datapoint const*> const& others
) const
{
	std::stringstream question_to_ask;
	question_to_ask << question_number << " ";
	question_to_ask << to_string(provided) << " is " << get(provided) << ", what is " << to_string(asked) << "?";
	if(!others.empty())
	{
		question_to_ask << " exclude: ";
		for(size_t i = 0; i < others.size(); ++i)
		{
			if(i>0) question_to_ask << ", ";
			question_to_ask << others[i]->get(asked);
		}
	}
	std::string usergave;
	usergave = input.do_input(question_to_ask.str());
	//switch(asked)
	//{
	//	case DATATYPE_CHINESE:
	//		break;
	//	case DATATYPE_ENGLISH:
	//		std::cin >> usergave;
	//		break;
	//	case DATATYPE_PINYIN:
	//		std::cin >> usergave;
	//		break;
	//}
	return usergave == get(asked);
}

bool questions::ask_all_until_fail() const
{
	std::vector<std::tuple<size_t, DATATYPE, DATATYPE>> myquestions;
	//PRINT(myquestions.size());
	for(size_t q=0; q<loaded_data.size(); ++q)
	{
		for(size_t i=0; i<3; ++i)
		{
			for(size_t j=0; j<3; ++j)
			{
				if(i==j) continue;
				myquestions.push_back(std::tuple<size_t, DATATYPE, DATATYPE>(q, (DATATYPE)i, (DATATYPE)j));
			}
		}
	}
	//PRINT(myquestions.size());
	std::mt19937 gen(std::chrono::steady_clock::now().time_since_epoch().count());
reshuffle:
	std::shuffle(myquestions.begin(), myquestions.end(), gen);
noreshuffle:
	//PRINT(myquestions.size());
	for(size_t q=0; q < myquestions.size() || q == size_t(-1); ++q)
	{
		std::tuple<size_t, DATATYPE, DATATYPE> which_q = myquestions[q];
		std::vector<datapoint const*> others;
		datapoint const* current = &loaded_data[std::get<0>(which_q)];
		DATATYPE provided = std::get<1>(which_q);
		DATATYPE asked = std::get<2>(which_q);
		if(provided==DATATYPE_PINYIN)
		{
			std::string key = current->get(DATATYPE_PINYIN);
			auto iter = pinyin_overlaps.find(key);
			if(iter != pinyin_overlaps.end())
			{
				for(size_t index: iter->second)
				{
					datapoint const* identical = &loaded_data[index];
					if(identical->get(asked) != current->get(asked))
					{
						others.push_back(identical);
					}
				}
			}
		}
		int repeat=3;
repeat_question:
		bool result =current->ask(
				m_input,
				provided,
				asked,
				std::to_string(q) + "/" + std::to_string(myquestions.size()),
				others
			);
		if(result && repeat== 3)
		{
			std::cout << "Answer accepted!\n";
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			repeat = 0;
		}
		else
		{
			if(!result)
			{
				std::cout << reset_color << "Wrong answer!\r\n";
				std::cout << "Correct would have been " << loaded_data[std::get<0>(which_q)].get(std::get<2>(which_q)) << "\r\n";
				std::cout << "Btw CHI=" << loaded_data[std::get<0>(which_q)].get(DATATYPE_CHINESE);
				std::cout << ", PYN=" << loaded_data[std::get<0>(which_q)].get(DATATYPE_PINYIN);
				std::cout << ", ENG=" << loaded_data[std::get<0>(which_q)].get(DATATYPE_ENGLISH) << "\r\n";
				getch();
			}
			else
			{
				std::cout << "Answer accepted!\n";
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
			}
			--repeat;
			if(repeat>0)
			{
				goto repeat_question;
			}
			else
			{
				q=size_t(-1);
				goto noreshuffle;
			}
			//move(0,0);
			//clear();
			//refresh();
			//return false;
		}
	}
	return true;
}

