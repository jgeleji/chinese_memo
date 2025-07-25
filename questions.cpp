#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <random>
#include <chrono>
#include <thread>
#include <cctype>

#include <ncurses.h>
#include <curses.h>

#include "questions.h"
#include "tokenize.h"

#define PRINT(X) std::cout << __FILE__ << ":" << __LINE__ << " " << (#X) << " = " << (X) << "\r\n"

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
		auto iter0 = question_index_finder.find(dp.chinese);
		if(iter0 != question_index_finder.end())
		{
			auto iter1 = iter0->second.find(dp.pinyin);
			if(iter1 != iter0->second.end())
			{
				auto iter2 = iter1->second.find(dp.english);
				if(iter2 != iter1->second.end())
				{
					continue;
				}
			}
		}
		loaded_data.push_back(dp);
		pinyin_overlaps[dp.pinyin].insert(loaded_data.size()-1);
		english_overlaps[dp.english].insert(loaded_data.size()-1);
		chinese_overlaps[dp.chinese].insert(loaded_data.size()-1);

		//std::tuple<std::string, std::string, std::string> key;
		question_index_finder[dp.chinese][dp.pinyin][dp.english] = loaded_data.size() - 1;
		//std::get<0>(key) = dp.chinese;
		//std::get<1>(key) = dp.pinyin;
		//std::get<2>(key) = dp.english;
		//question_index_finder[key] = loaded_data.size() - 1;
	}

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
			return "\033[38;5;226mchinese\033[0m";
		case DATATYPE_PINYIN:
			return "\033[38;5;236mpinyin\033[0m";
		case DATATYPE_ENGLISH:
			return "\033[38;5;27menglish\033[0m";
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
	std::string& gave,
	int breaks
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
			gave = input.do_input_chinese(question_to_ask.str(), true, breaks, chinese.c_str());
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

void questions::statistics_screen(
	std::map<questions::q_type, std::pair<size_t, double>> const& recurrence_scores,
	int breaks
) const
{
	PRINT(loaded_data.size());
	PRINT(recurrence_scores.size());
	size_t zero_score=0;
	std::map<int, int> positive_score;
	std::map<int, int> negative_score;
	long double total_score=0.0l;
	bool have_overloaded = false;
	for(auto iter = recurrence_scores.begin(); iter != recurrence_scores.end(); ++iter)
	{
		double score = iter->second.second;
		if(false && score < -4)
		{
			have_overloaded = true;
			std::cout << "Overloaded score(" << score << "). ";
			std::cout << loaded_data[std::get<0>(iter->first)].chinese;
			int provided = std::get<1>(iter->first);
			int asked = std::get<2>(iter->first);
			std::cout << " " << provided;
			std::cout << " " << asked;
			std::cout << "\r\n";
			std::fstream statusfile;
			statusfile.open("status.txt", std::fstream::out | std::fstream::app);
			statusfile << loaded_data[std::get<0>(iter->first)].chinese;
			statusfile << "|";
			statusfile << loaded_data[std::get<0>(iter->first)].pinyin;
			statusfile << "|";
			statusfile << loaded_data[std::get<0>(iter->first)].english;
			statusfile << "|" << ((int)provided);
			statusfile << "|" << ((int)asked);
			statusfile << "|0|fail\n";
		}
		total_score += score;
		if(score < 0.0)
		{
			++negative_score[floor(score)];
		}
		else if(score == 0.0)
		{
			++zero_score;
		}
		else
		{
			++positive_score[ceil(score)];
		}
	}
	if(have_overloaded)	std::cin.get();
	for(auto iter = negative_score.begin(); iter != negative_score.end(); ++iter)
	{
		PRINT(iter->first);
		PRINT(iter->second);
	}
	PRINT(negative_score.size());
	PRINT(zero_score);
	PRINT(positive_score.size());
	for(auto iter = positive_score.begin(); iter != positive_score.end(); ++iter)
	{
		PRINT(iter->first);
		PRINT(iter->second);
	}

	PRINT(total_score);
	PRINT(total_score/recurrence_scores.size());
	PRINT(COLS);
	PRINT(breaks);
	std::cin.get();
}

bool questions::ask_all_until_fail(int breaks) const
{
	std::mt19937 gen(std::chrono::steady_clock::now().time_since_epoch().count());
	double score_improve_if_success = 1.04;
	double score_deteriorate_if_fail = 5.19;
	double max_complexity = 6.33;

	//std::vector<q_type> myquestions;
						// when it was asked, score(higher number means should be asked sooner)
	std::map<q_type, std::pair<size_t, double>> recurrence_scores;
	//PRINT(myquestions.size());
	for(size_t q=0; q<loaded_data.size(); ++q)
	{
		for(size_t i=0; i<3; ++i)
		{
			for(size_t j=0; j<3; ++j)
			{
				if(i==j) continue;
				q_type val = q_type(q, (DATATYPE)i, (DATATYPE)j);
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
		if(tokens.size() < 6)
		{
			continue;
		}
		std::string chinese = tokens[0];
		std::string maybe_pinyin = tokens[1];
		std::string maybe_english = tokens[2];
		auto iter0 = question_index_finder.find(chinese);
		if(iter0 == question_index_finder.end())
		{
			continue;
		}
		auto iter1 = iter0->second.find(maybe_pinyin);
		if(iter1 == iter0->second.end())
		{
			iter1 = iter0->second.begin();
		}
		auto iter2 = iter1->second.find(maybe_english);
		if(iter2 == iter1->second.end())
		{
			iter2 = iter1->second.begin();
		}
		q_type key = q_type(
			iter2->second,
			(DATATYPE)atoi(tokens[3].c_str()),
			(DATATYPE)atoi(tokens[4].c_str())
		);
		auto iter = recurrence_scores.find(key);
		iter->second.first = std::max(iter->second.first, (size_t)atoll(tokens[5].c_str()));
		sequence_number = std::max(sequence_number, iter->second.first);
		if(tokens[6] == "success")
		{
			iter->second.second -= score_improve_if_success + (gen()%5)*.01;
		}
		else if(tokens[6] == "fail")
		{
			iter->second.second = std::min(
				std::max(
					(int)4,
					(int)(max_complexity*tokens[0].size()/6)
				),
				(int)(iter->second.second + score_deteriorate_if_fail + (gen()%5000)*.00001)
			);
		}


	}
	statistics_screen(recurrence_scores, breaks);

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
			if(iter->second.second < 0)
			{
				int tmp = iter->second.second;
				skipdistance = 40;
				while(tmp++ < 0)
				{
					skipdistance *= 2;
				}
			}
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

		
		std::string key = current->get(provided);
		//auto iter = pinyin_overlaps.find(key);
		std::unordered_map<std::string, std::unordered_set<int>>::const_iterator iter;
		std::unordered_map<std::string, std::unordered_set<int>> const* provided_overlaps, *asked_overlaps;
		switch(asked)
		{
			case DATATYPE_PINYIN:
				asked_overlaps = &pinyin_overlaps;
				break;
			case DATATYPE_ENGLISH:
				asked_overlaps = &english_overlaps;
				break;
			case DATATYPE_CHINESE:
				asked_overlaps = &chinese_overlaps;
				break;
		}
		switch(provided)
		{
			case DATATYPE_PINYIN:
				provided_overlaps = &pinyin_overlaps;
				break;
			case DATATYPE_ENGLISH:
				provided_overlaps = &english_overlaps;
				break;
			case DATATYPE_CHINESE:
				provided_overlaps = &chinese_overlaps;
				break;
		}
		iter = provided_overlaps->find(key);
		if(iter != provided_overlaps->end())
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
				gave,
				breaks
			);
		iter0->second.first = sequence_number;
		statusfile.open("status.txt", std::fstream::out | std::fstream::app);
		statusfile << current->get(DATATYPE_CHINESE);
		statusfile << "|";
		statusfile << current->get(DATATYPE_PINYIN);
		statusfile << "|";
		statusfile << current->get(DATATYPE_ENGLISH);
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
			iter0->second.second = std::min(
				(int)(iter0->second.second + score_deteriorate_if_fail+(gen()%5000)*.00001),
				std::max(
					(int)4,
					(int)(max_complexity* current->get(DATATYPE_CHINESE).size()/6)
				)
			);
			auto iter = asked_overlaps->find(gave);
;
			if(iter != asked_overlaps->end())
			{
				for(size_t index: iter->second)
				{
					//to punish the question also where the answer would have been correct!
		//datapoint const* current = &loaded_data[std::get<0>(which_q)];
		//DATATYPE provided = std::get<1>(which_q);
		//DATATYPE asked = std::get<2>(which_q);
					datapoint const* punishable = &loaded_data[index];
					DATATYPE punishable_provided = asked;
					DATATYPE punishable_asked = provided;

					q_type val = q_type(index, asked, provided);
					recurrence_scores[val].second = std::min(recurrence_scores[val].second + score_deteriorate_if_fail+(gen()%5000)*.00001, max_complexity* punishable->get(DATATYPE_CHINESE).size()/6);
					statusfile << "\n";
					statusfile << punishable->get(DATATYPE_CHINESE);
					statusfile << "|";
					statusfile << punishable->get(DATATYPE_PINYIN);
					statusfile << "|";
					statusfile << punishable->get(DATATYPE_ENGLISH);
					statusfile << "|" << ((int)punishable_provided);
					statusfile << "|" << ((int)punishable_asked);
					statusfile << "|" << sequence_number << "|fail";
					//datapoint const* identical = &loaded_data[index];
					//if(identical->get(asked) != current->get(asked))
					//{
					//	others.push_back(identical);
					//}
				}
			}
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

