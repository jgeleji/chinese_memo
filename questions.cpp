#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <random>
#include <chrono>
#include <thread>
#include <cctype>
#include <map>

#include <ncurses.h>
#include <curses.h>

#include "questions.h"
#include "tokenize.h"

#define PRINT(X) std::cout << __FILE__ << ":" << __LINE__ << " " << (#X) << " = " << (X) << std::endl


std::string tolower(std::string data = "Abc")
{
	std::transform(data.begin(), data.end(), data.begin(),
		[](unsigned char c){ return std::tolower(c); });
	return data;
}

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
		dp.english = tolower(tokens[1]);
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
		english_overlaps[dp.english].insert(loaded_data.size()-1);
		chinese_overlaps[dp.chinese].insert(loaded_data.size()-1);
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
	std::string gave;
	bool result = loaded_data[which].ask(m_input, provided, asked, std::string(), others, gave);
	std::cout << "Received " << gave << "\n";
	return result;
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
	std::vector<datapoint const*> const& others,
	std::string& gave
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
	switch(asked)
	{
		case DATATYPE_CHINESE:
			gave = input.do_input_chinese(question_to_ask.str(), true);
			break;
		case DATATYPE_ENGLISH:
			gave = tolower(input.do_input_english(question_to_ask.str()));
			break;
		case DATATYPE_PINYIN:
			gave = input.do_input_pinyin(question_to_ask.str());
			break;
	}
	return gave == get(asked);
}

bool questions::ask_all_until_fail() const
{
	std::mt19937 gen(std::chrono::steady_clock::now().time_since_epoch().count());
	double score_improve_if_success = 1.04;
	double score_deteriorate_if_fail = 5.19;
	double max_complexity = 6.33;

	typedef std::tuple<size_t, DATATYPE, DATATYPE> q_type;
	//std::vector<q_type> myquestions;
						// when it was asked, score(higher number means should be asked sooner)
	std::map<q_type, std::pair<size_t, double>> recurrence_scores;
	std::map<std::string, size_t> q_to_index;//use chinese as string
	//PRINT(myquestions.size());
	for(size_t q=0; q<loaded_data.size(); ++q)
	{
		for(size_t i=0; i<3; ++i)
		{
			for(size_t j=0; j<3; ++j)
			{
				if(i==j) continue;
				q_type val = q_type(q, (DATATYPE)i, (DATATYPE)j);
				std::string chinese = loaded_data[q].get(DATATYPE_CHINESE);
				q_to_index[chinese] = q;
				//myquestions.push_back(val);
				recurrence_scores[val] = std::pair<size_t, double>(0, 0.0);
			}
		}
	}

	size_t sequence_number = 0;
	std::fstream statusfile;
	statusfile.open("status.txt", std::fstream::in);
	while(statusfile.good())
	{
		std::string line;
		getline(statusfile, line);
		std::vector<std::string> tokens = tokenize(line, '|');
		//chinese
		//datatype provided
		//datatype asked
		//when it was asked ordinal number
		//was it answered successfully/failed?
		if(tokens.size() < 5)
		{
			continue;
		}
		std::tuple<std::string, DATATYPE, DATATYPE> index_finder_key = std::tuple<std::string, DATATYPE, DATATYPE>(
			tokens[0],
			(DATATYPE)atoi(tokens[1].c_str()),
			(DATATYPE)atoi(tokens[2].c_str())
		);
		q_type key = q_type(
			q_to_index[tokens[0]],
			(DATATYPE)atoi(tokens[1].c_str()),
			(DATATYPE)atoi(tokens[2].c_str())
		);
		auto iter = recurrence_scores.find(key);
		iter->second.first = std::max(iter->second.first, (size_t)atoll(tokens[3].c_str()));
		sequence_number = std::max(sequence_number, iter->second.first);
		if(tokens[4] == "success")
		{
			iter->second.second -= score_improve_if_success + (gen()%5)*.01;
		}
		else if(tokens[4] == "fail")
		{
			iter->second.second = std::min(max_complexity*tokens[0].size()/2, iter->second.second + score_deteriorate_if_fail + (gen()%5000)*.00001);
		}


	}

	//PRINT(myquestions.size());
reshuffle:
	//std::shuffle(myquestions.begin(), myquestions.end(), gen);
noreshuffle:
	//PRINT(myquestions.size());
	//for(size_t q=0; q < myquestions.size() || q == size_t(-1); ++q)
	while(1)
	{
		++sequence_number;

		std::vector<std::map<q_type, std::pair<size_t, double>>::iterator> equal_chances;
		auto iter0 = recurrence_scores.begin();
		equal_chances.push_back(iter0);
		for(auto iter = recurrence_scores.begin(); iter != recurrence_scores.end(); ++iter)
		{
			int skipdistance = 10;
			if(iter->second.second < 2.5) skipdistance = 17;
			if(iter->second.second < 1.1) skipdistance = 29;
			if(iter->second.second < .5) skipdistance = 36;
			if(iter->second.first + skipdistance > sequence_number) continue;
			if(iter->second.second > iter0->second.second)
			{
				iter0 = iter;
				equal_chances.clear();
				equal_chances.push_back(iter);
			}
			else if(iter->second.second == iter0->second.second)
			{
				equal_chances.push_back(iter);
			}
		}
		std::shuffle(equal_chances.begin(), equal_chances.end(), gen);
		iter0 = equal_chances.front();

		//std::tuple<std::string, DATATYPE, DATATYPE> which_q_0 = iter0->first;

		//std::tuple<size_t, DATATYPE, DATATYPE> which_q = std::tuple<size_t, DATATYPE, DATATYPE>(
		//	q_to_index[std::get<0>(which_q_0)],
		//	std::get<1>(which_q_0),
		//	std::get<2>(which_q_0)
		//);//myquestions[q];
		q_type which_q = iter0->first;
		size_t q = std::get<0>(which_q);


		std::vector<datapoint const*> others;
		datapoint const* current = &loaded_data[std::get<0>(which_q)];
		DATATYPE provided = std::get<1>(which_q);
		DATATYPE asked = std::get<2>(which_q);

		{
			std::string key = current->get(provided);
			//auto iter = pinyin_overlaps.find(key);
			std::unordered_map<std::string, std::unordered_set<int>>::const_iterator iter;
			switch(provided)
			{
				case DATATYPE_PINYIN:
					iter = pinyin_overlaps.find(key);
					break;
				case DATATYPE_ENGLISH:
					iter = english_overlaps.find(key);
					break;
				case DATATYPE_CHINESE:
					iter = chinese_overlaps.find(key);
					break;
			}
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
		std::string gave;
		bool result =current->ask(
				m_input,
				provided,
				asked,
				std::to_string(q)
				+ "/" + std::to_string(6*loaded_data.size())
				+ " " + std::to_string(sequence_number)
				+ " " + std::to_string(iter0->second.first)
				+ " " + std::to_string(iter0->second.second),
				others,
				gave
			);
		iter0->second.first = sequence_number;
		statusfile.open("status.txt", std::fstream::out | std::fstream::app);
		statusfile << current->get(DATATYPE_CHINESE);
		statusfile << "|" << ((int)provided);
		statusfile << "|" << ((int)asked);
		statusfile << "|" << sequence_number << "|";
		if(result)
		{
			statusfile << "success";
			iter0->second.second -= score_improve_if_success +(gen()%5)*.01;
		}
		else
		{
			statusfile << "fail";
			iter0->second.second = std::min(iter0->second.second + score_deteriorate_if_fail+(gen()%5000)*.00001, max_complexity* current->get(DATATYPE_CHINESE).size()/2);
;

		}
		statusfile << "\n";
		statusfile.close();
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
				std::cout << reset_color << "Wrong answer (" << gave << ")!\r\n";
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

