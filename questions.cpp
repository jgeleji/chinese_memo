#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <random>
#include <chrono>
#include <thread>
#include <cctype>

#include "questions.h"
#include "tokenize.h"

#define PRINT(X) std::cout << __FILE__ << ":" << __LINE__ << " " << (#X) << " = " << (X) << "\r\n"

std::string tolower(std::string data = "Abc")
{
	std::transform(data.begin(), data.end(), data.begin(),
		[](unsigned char c){ return std::tolower(c); });
	return data;
}

std::string nospace(std::string data)
{
	data.erase(std::remove(data.begin(), data.end(), ' '), data.end());
	return data;
}

chinese::questions::questions(std::string statusfilename) : statusfilename(statusfilename)
{
}

void chinese::questions::load_file_hsk(std::string const& filename)
{
	std::fstream infile;
	infile.open(filename, std::fstream::in);
	size_t line_no = 0;
	while(infile.good())
	{
		std::string line;
		getline(infile, line);
		++line_no;
		std::vector<std::string> tokens;// = tokenize(line, '|');
		try {
			size_t pos0;
			pos0 = line.find_first_of(")");
			if(pos0 == line.npos)
			{
				pos0 = line.find_first_of(" ");
				if(pos0 == line.npos) continue;
			}
			else
			{
				++pos0;
			}
			tokens.push_back(line.substr(0, pos0));
			line = line.substr(pos0 + 1);
			pos0 = line.find_first_of(")");
			if(pos0 == line.npos)
			{
				pos0 = line.find_first_of(" ");
				if(pos0 == line.npos) continue;
			}
			else
			{
				++pos0;
			}
			tokens.push_back(line.substr(pos0+1));
			tokens.push_back(line.substr(0, pos0));
			for(int i=0; i<tokens.size(); ++i)
			{
				while(!tokens[i].empty() && tokens[i][0] == ' ')
				{
					tokens[i].erase(0, 1);
				}
				while(!tokens[i].empty() && tokens[i][tokens[i].size()-1]==' ')
				{
					tokens[i].erase(tokens[i].size() - 1, 1);
				}
			}
		}
		catch(std::exception const& e)
		{
		}

		if(tokens.size() < 3) continue;
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

void chinese::questions::load_file(std::string const& filename)
{
	std::fstream infile;
	infile.open(filename, std::fstream::in);
	size_t line_no = 0;
	while(infile.good())
	{
		std::string line;
		getline(infile, line);
		++line_no;
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

std::string chinese::questions::to_string(DATATYPE val)
{
	switch(val)
	{
		case DATATYPE_CHINESE:
			return "\033[38;5;226mchinese\033[0m";
		case DATATYPE_PINYIN:
			return "\033[38;5;242mpinyin\033[0m";
		case DATATYPE_ENGLISH:
			return "\033[38;5;27menglish\033[0m";
	}
	return "unknown";
}

std::string const& chinese::questions::datapoint::get(DATATYPE which) const
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

bool chinese::questions::datapoint::ask(
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
			gave = input.do_input_english(question_to_ask.str());
			break;
		case DATATYPE_PINYIN:
			gave = input.do_input_pinyin(question_to_ask.str());
			break;
	}
	return nospace(tolower(gave)) == nospace(tolower(get(asked)));
}

void chinese::questions::statistics_screen(
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
			statusfile.open(statusfilename, std::fstream::out | std::fstream::app);
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
	size_t total_positive = 0;
	for(auto iter = positive_score.begin(); iter != positive_score.end(); ++iter)
	{
		total_positive += iter->second;
	}
	std::cout << "Total positive score: " << total_positive << "\r\n";
	size_t total_negative = 0;
	for(auto iter = negative_score.begin(); iter != negative_score.end(); ++iter)
	{
		total_negative += iter->second;
	}
	std::cout << "Total negative score: " << total_negative << "\r\n";
	for(auto iter = negative_score.begin(); iter != negative_score.end(); ++iter)
	{
		std::cout << "Negative score: " << iter->first << " count: " << iter->second << "\r\n";
	}
	PRINT(negative_score.size());
	PRINT(zero_score);
	PRINT(positive_score.size());
	for(auto iter = positive_score.begin(); iter != positive_score.end(); ++iter)
	{
		std::cout << "Positive score: " << iter->first << " count: " << iter->second << "\r\n";
	}

	PRINT(total_score);
	PRINT(total_score/recurrence_scores.size());
	PRINT(m_input.cols());
	PRINT(breaks);
	std::cin.get();
}

std::map<chinese::questions::q_type, std::pair<size_t, double>> chinese::questions::load_status_file(
	std::mt19937& gen,
	size_t& sequence_number,
	double score_improve_if_success,
	double score_deteriorate_if_fail,
	double max_complexity
) const
{
	//std::vector<q_type> myquestions;
						// when it was asked, score(higher number means should be asked sooner)
	std::map<q_type, std::pair<size_t, double>> recurrence_scores;
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

	sequence_number = 0;
	std::fstream statusfile;
	statusfile.open(statusfilename, std::fstream::in);
	long double global_total = 0.0;
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
			double change = -1* score_improve_if_success + (gen()%5)*.01;
			double before = iter->second.second;
			
			iter->second.second = std::max(before + change,
					(double)global_total/recurrence_scores.size() - 15
				);
			global_total += change;
		}
		else if(tokens[6] == "fail")
		{
			typedef int mintype;
			double before = iter->second.second;
			iter->second.second = 
				std::min(
					std::max(
						(mintype)4,
						(mintype)(max_complexity*tokens[0].size()/6)
					),
					(mintype)(iter->second.second + score_deteriorate_if_fail + (gen()%5000)*.00001)
				);
			double after = iter->second.second;
			global_total += after - before;
		}


	}
	return recurrence_scores;
}

bool chinese::questions::ask_all_until_fail_block10(int breaks) const
{
	std::mt19937 gen(1337);//std::chrono::steady_clock::now().time_since_epoch().count());
	double score_improve_if_success = 1.04;
	double score_deteriorate_if_fail = 5.19;
	double max_complexity = 6.33;

	size_t sequence_number = 0;

	auto recurrence_scores = load_status_file(gen, sequence_number, score_improve_if_success, score_deteriorate_if_fail, max_complexity);

	statistics_screen(recurrence_scores, breaks);

//noreshuffle:
	std::multimap<double, std::map<q_type, std::pair<size_t, double>>::iterator> questions_block;
	auto iter0 = recurrence_scores.begin();
	questions_block.insert(std::pair<double, std::map<q_type, std::pair<size_t, double>>::iterator>(iter0->second.second, iter0));
	for(auto iter = recurrence_scores.begin(); iter != recurrence_scores.end(); ++iter)
	{
		questions_block.insert(std::pair<double, std::map<q_type, std::pair<size_t, double>>::iterator>(iter->second.second, iter));
	}
	while(questions_block.size() > 15)
	{
		int max_erase_index=0;
		const double erase_val = questions_block.begin()->first;
		auto max_erase_iter = questions_block.begin();
		while(max_erase_iter->first <= erase_val && max_erase_iter != questions_block.end())
		{
			++max_erase_index;
			++max_erase_iter;
		}
		int erase_which = 0;
		if(max_erase_index > 0)
		{
			erase_which = gen()%max_erase_index;
		}
		auto erase_iter = questions_block.begin();
		std::advance(erase_iter, erase_which );
		questions_block.erase(erase_iter);
	}

	std::vector<std::map<q_type, std::pair<size_t, double>>::iterator> equal_chances; // rev
	for(auto iter = questions_block.rbegin(); iter !=  questions_block.rend() ; ++iter)
	{
		equal_chances.push_back(iter->second);
	}
	//std::shuffle(equal_chances.begin(), equal_chances.end(), gen);
	//std::reverse(equal_chances.begin(), equal_chances.end());
	int loc_seq=0;
	while(1)
	{
		++sequence_number;

		auto iter00 = equal_chances.begin();
		std::advance(iter00, loc_seq%equal_chances.size());
		iter0 = *iter00;

		q_type which_q = iter0->first;

		datapoint const* current = &loaded_data[std::get<0>(which_q)];

		size_t q = std::get<0>(which_q);
		std::vector<datapoint const*> others;
		DATATYPE provided = std::get<1>(which_q);
		DATATYPE asked = std::get<2>(which_q);

		
		std::string key = current->get(provided);
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

		std::string gave;

		std::map<int, int> negative_score, positive_score;

		negative_score.clear();
		positive_score.clear();

		int zero_score = 0;
		for(auto iter = recurrence_scores.begin(); iter != recurrence_scores.end(); ++iter)
		{
			double score = iter->second.second;
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

		std::string stats = 
				reset_color
				+ std::to_string(loc_seq)
				+ " " + std::to_string(q)
				+ "/" + std::to_string(6*loaded_data.size())
				+ " " + std::to_string(sequence_number)
				+ " " + std::to_string(iter0->second.first)
				+ " " + std::to_string(iter0->second.second);
		if(zero_score >0)
		{
			stats += " zero " + std::to_string(zero_score);
		}
		bool result =current->ask(
				m_input,
				provided,
				asked,
				stats,
				others,
				gave,
				breaks
			);
		iter0->second.first = sequence_number;
		if(result)
		{
			++loc_seq;
			std::cout << "Answer accepted!\n";
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
		else
		{
			loc_seq = 0;
			std::cout << reset_color << "Wrong answer (" << gave << ")!\r\n";
			std::cout << "Correct would have been " << loaded_data[std::get<0>(which_q)].get(std::get<2>(which_q)) << "\r\n";
			std::cout << "Btw CHI=" << loaded_data[std::get<0>(which_q)].get(DATATYPE_CHINESE);
			std::cout << ", PYN=" << loaded_data[std::get<0>(which_q)].get(DATATYPE_PINYIN);
			std::cout << ", ENG=" << loaded_data[std::get<0>(which_q)].get(DATATYPE_ENGLISH) << "\r\n";
			//getch();
			system_pause();
		}
		if(loc_seq > 0 && loc_seq % questions_block.size() == 0)
		{
			std::fstream statusfile;
			statusfile.open(statusfilename, std::fstream::out | std::fstream::app);
			for(auto iter = questions_block.begin(); iter != questions_block.end(); ++iter)
			{
				q_type which_q = iter->second->first;

				DATATYPE provided = std::get<1>(which_q);
				DATATYPE asked = std::get<2>(which_q);

				datapoint const* current = &loaded_data[std::get<0>(which_q)];

				statusfile << current->get(DATATYPE_CHINESE);
				statusfile << "|";
				statusfile << current->get(DATATYPE_PINYIN);
				statusfile << "|";
				statusfile << current->get(DATATYPE_ENGLISH);
				statusfile << "|" << ((int)provided);
				statusfile << "|" << ((int)asked);
				statusfile << "|" << sequence_number << "|";
				statusfile << "success";
				iter->second->second.second -= score_improve_if_success +(gen()%5)*.01;
				statusfile << "\n";
			}
			statusfile.close();
		}
	}
	return true;
}

bool chinese::questions::ask_all_until_fail(int breaks) const
{
	std::mt19937 gen(1337);//std::chrono::steady_clock::now().time_since_epoch().count());
	double score_improve_if_success = 1.04;
	double score_deteriorate_if_fail = 5.19;
	double max_complexity = 6.33;

	size_t sequence_number = 0;

	auto recurrence_scores = load_status_file(gen, sequence_number, score_improve_if_success, score_deteriorate_if_fail, max_complexity);

	statistics_screen(recurrence_scores, breaks);

//noreshuffle:
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

		q_type which_q = iter0->first;
		size_t q = std::get<0>(which_q);


		std::vector<datapoint const*> others;
		datapoint const* current = &loaded_data[std::get<0>(which_q)];
		DATATYPE provided = std::get<1>(which_q);
		DATATYPE asked = std::get<2>(which_q);

		
		std::string key = current->get(provided);
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

		std::map<int, int> negative_score, positive_score;

		negative_score.clear();
		positive_score.clear();

		int zero_score = 0;
		for(auto iter = recurrence_scores.begin(); iter != recurrence_scores.end(); ++iter)
		{
			double score = iter->second.second;
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

		std::string stats = 
				std::to_string(q)
				+ "/" + std::to_string(6*loaded_data.size())
				+ " " + std::to_string(sequence_number)
				+ " " + std::to_string(iter0->second.first)
				+ " " + std::to_string(iter0->second.second);
		if(zero_score >0)
		{
			stats += " zero " + std::to_string(zero_score);
		}
		bool result =current->ask(
				m_input,
				provided,
				asked,
				stats,
				others,
				gave,
				breaks
			);
		iter0->second.first = sequence_number;
		std::fstream statusfile;
		statusfile.open(statusfilename, std::fstream::out | std::fstream::app);
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
			if(iter != asked_overlaps->end())
			{
				for(size_t index: iter->second)
				{
					//to punish the question also where the answer would have been correct!
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
				}
			}
		}
		statusfile << "\n";
		statusfile.close();
		if(result && repeat== 3)
		{
			std::cout << "Answer accepted!\n";
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
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
				//getch();
				system_pause();
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
				//q=size_t(-1);
				//goto noreshuffle;
			}
		}
	}
	return true;
}

void chinese::questions::populate_chinese_char_to_index()
{
	chinese_char_to_index.clear();
	for(size_t i = 0; i < loaded_data.size(); ++i)
	{
		const std::string& chinese = loaded_data[i].chinese;
		std::vector	<std::string> chinese_chars = m_input.split_into_chinese_characters(chinese);
		for(size_t j = 0; j < chinese_chars.size(); ++j)
		{
			chinese_char_to_index[chinese_chars[j]].insert(i);
		}
	}
}

bool chinese::questions::ask_1_chinese_char(
	std::string question_number,
	std::string const& chinese_char,
	DATATYPE asked,
	int breaks
) const
{
	if(chinese_char.empty())
	{
		std::cout << "Empty chinese character!\n";
		return false;
	}
	auto iter = chinese_char_to_index.find(chinese_char);
	if(iter == chinese_char_to_index.end())
	{
		std::cout << "Unknown chinese character: " << chinese_char << "\n";
		return false;
	}
	std::vector<datapoint const*> good_answers_remaining;
	std::vector<datapoint const*> good_answers_already_given;
	for(size_t index: iter->second)
	{
		good_answers_remaining.push_back(&loaded_data[index]);
	}
	while(good_answers_remaining.size())
	{
		std::stringstream question_to_ask;
		question_to_ask << question_number << " ";
		question_to_ask << "Character is " << chinese_char << ", what is " << to_string(asked) << "? The number of remaining good answers is " << good_answers_remaining.size() << ".";
		question_to_ask << " Already given: ";
		for(size_t i = 0; i < good_answers_already_given.size(); ++i)
		{
			if(i > 0) question_to_ask << ", ";
			question_to_ask << good_answers_already_given[i]->get(asked);
		}
		std::string gave;
		std::stringstream chinese;
		for(size_t i = 0; i < good_answers_remaining.size(); ++i)
		{
			chinese << good_answers_remaining[i]->get(DATATYPE_CHINESE);
		}
		switch(asked)
		{
			case DATATYPE_CHINESE:
				gave = m_input.do_input_chinese(question_to_ask.str(), true, breaks, chinese.str().c_str());
				break;
			case DATATYPE_ENGLISH:
				gave = tolower(m_input.do_input_english(question_to_ask.str()));
				break;
			case DATATYPE_PINYIN:
				gave = m_input.do_input_pinyin(question_to_ask.str());
				break;
		}
		bool found = false;
		for(size_t i = 0; i < good_answers_remaining.size(); ++i)
		{
			if(gave == good_answers_remaining[i]->get(asked))
			{
				std::cout << "Answer accepted!\n";
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
				good_answers_already_given.push_back(good_answers_remaining[i]);
				good_answers_remaining.erase(good_answers_remaining.begin() + i);
				found = true;
				break;
			}
		}
		if(!found)
		{
			std::cout << reset_color << "Wrong answer (" << gave << ")!\r\n";
			std::cout << "Correct would have been one of: ";
			for(size_t i = 0; i < good_answers_remaining.size(); ++i)
			{
				//std::cout << good_answers_remaining[i]->get(asked) << " ";
				std::cout << good_answers_remaining[i]->get(DATATYPE_CHINESE) << " ("
				          << good_answers_remaining[i]->get(DATATYPE_PINYIN) << ", "
				          << good_answers_remaining[i]->get(DATATYPE_ENGLISH) << ") ";
			}
			std::cout << "\r\n";
			//getch();
			system_pause();
			break;
		}
	}
	return good_answers_remaining.size() == 0;
}

void chinese::questions::character_statistics_screen(
	std::map<chinese::questions::q_type_char, std::pair<size_t, double>> const& recurrence_scores,
	int breaks,
	size_t sequence_number
) const
{
	std::cout << "Chinese character statistics:\r\n";
	size_t zero_score=0;
	std::map<int, int> positive_score;
	std::map<int, int> negative_score;
	long double total_score=0.0l;
	bool have_overloaded = false;
	size_t total_positive = 0;
	size_t total_negative = 0;
	PRINT(recurrence_scores.size());
	for(auto iter = recurrence_scores.begin(); iter != recurrence_scores.end(); ++iter)
	{
		double score = iter->second.second;
		if(false && score < -4)
		{
			have_overloaded = true;
			std::cout << "Overloaded score(" << score << "). ";
			std::cout << iter->first.first << ", " << iter->first.second  << "\r\n";
		}
		total_score += score;
		if(score < 0.0)
		{
			++negative_score[floor(score)];
			++total_negative;
		}
		else if(score == 0.0)
		{
			++zero_score;
		}
		else
		{
			++positive_score[ceil(score)];
			++total_positive;
		}
	}
	std::cout << "Total positive score: " << total_positive << "\r\n";
	std::cout << "Total negative score: " << total_negative << "\r\n";
	for(auto iter = negative_score.begin(); iter != negative_score.end(); ++iter)
	{
		std::cout << "Negative score: " << iter->first << " count: " << iter->second << "\r\n";
	}
	std::cout << "Zero score: " << zero_score << "\r\n";
	for(auto iter = positive_score.begin(); iter != positive_score.end(); ++iter)
	{
		std::cout << "Positive score: " << iter->first << " count: " << iter->second << "\r\n";
	}


	std::cout << "Total chinese characters: " << chinese_char_to_index.size() << "\r\n";

	std::cout << "Total score: " << total_score << "\r\n";
	std::cout << "Average score per character: "
	          << (total_score/recurrence_scores.size()) 
	          << "\r\n";
	std::cout << "Sequence number: " << sequence_number << "\r\n";

	if(cols() > 80 && breaks > 0)
	{
		std::cout << "Press any key to continue...\r\n";
		//getch();
		//move(0,0);
		//clear();
		//refresh();
	}
	else
	{
		std::cout << "Press enter to continue...\r\n";
		//std::cin.get();
		//move(0,0);
		//clear();
		//refresh();
	}
	std::cin.get();
}

std::map<chinese::questions::q_type_char, std::pair<size_t, double>> chinese::questions::load_status_file_char(
	std::mt19937& gen,
	size_t& sequence_number,
	double score_improve_if_success,
	double score_deteriorate_if_fail,
	double max_complexity
) const
{
	std::map<chinese::questions::q_type_char, std::pair<size_t, double>> recurrence_scores;
	for(size_t q=0; q<loaded_data.size(); ++q)
	{
		std::vector<std::string> chinese_chars = m_input.split_into_chinese_characters(loaded_data[q].chinese);
		for(const std::string& chinese_char: chinese_chars)
		{
			recurrence_scores[q_type_char(chinese_char, DATATYPE_ENGLISH)] = std::pair<size_t, double>(0, 0.0);
			recurrence_scores[q_type_char(chinese_char, DATATYPE_PINYIN)] = std::pair<size_t, double>(0, 0.0);
			recurrence_scores[q_type_char(chinese_char, DATATYPE_CHINESE)] = std::pair<size_t, double>(0, 0.0);
		}
	}

	// the meaning of the tokens
	// 0: chinese character
	// 1: asked datatype (0=chinese, 1=pinyin, 2=english)
	// 2: sequence number (when it was asked)
	// 3: outcome (success or fail)


	sequence_number = 0;
	std::fstream statusfile;
	statusfile.open("status_c.txt", std::fstream::in);
	while(statusfile.good())
	{
		std::string line;
		getline(statusfile, line);
		std::vector<std::string> tokens = tokenize(line, '|');
		if(tokens.size() < 4)
		{
			continue;
		}
		std::string chinese_char = tokens[0];
		DATATYPE asked = (DATATYPE)atoi(tokens[1].c_str());
		q_type_char key = q_type_char(chinese_char, asked);
		auto iter = recurrence_scores.find(key);
		if(iter == recurrence_scores.end())
		{
			std::cout << "Unknown chinese character: " << chinese_char << "\n";
			continue;
		}
		iter->second.first = std::max(iter->second.first, (size_t)atoll(tokens[2].c_str()));
		sequence_number = std::max(sequence_number, iter->second.first);
		if(tokens[3] == "success")
		{
			iter->second.second -= score_improve_if_success + (gen()%5)*.01;
		}
		else if(tokens[3] == "fail")
		{
			iter->second.second = std::min(
				std::max(
					(int)4,
					(int)(max_complexity*chinese_char.size()/6)
				),
				int(iter->second.second + score_deteriorate_if_fail + (gen()%5000)*.00001)
			);
		}
		
	}
	return recurrence_scores;
}

bool chinese::questions::ask_all_chinese_chars(
	int breaks
) const
{
	std::mt19937 gen(1337);//std::chrono::steady_clock::now().time_since_epoch().count());
	double score_improve_if_success = 1.04;
	double score_deteriorate_if_fail = 5.19;
	double max_complexity = 6.33;


	size_t sequence_number = 0;
	std::map<q_type_char, std::pair<size_t, double>> recurrence_scores;
	recurrence_scores = load_status_file_char(gen, sequence_number, score_improve_if_success, score_deteriorate_if_fail, max_complexity);

	character_statistics_screen(recurrence_scores, breaks, sequence_number);

	while(1)
	{
		++sequence_number;

		std::vector<std::map<q_type_char, std::pair<size_t, double>>::iterator> equal_chances;
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



		int repeat=3;
repeat_question:
		std::stringstream question_number;
		question_number.str("");
		question_number << iter0->first.first << "/" << chinese_char_to_index.size() << " "
			<< std::to_string(sequence_number) << " "
			<< std::to_string(iter0->second.first) << " "
			<< std::to_string(iter0->second.second);


		bool outcome = ask_1_chinese_char(
			question_number.str(),
			iter0->first.first,
			DATATYPE(iter0->first.second),
			breaks
		);
		iter0->second.first = sequence_number;
	// the meaning of the tokens
	// 0: chinese character
	// 1: asked datatype (0=chinese, 1=pinyin, 2=english)
	// 2: sequence number (when it was asked)
	// 3: outcome (success or fail)

		std::fstream statusfile;
		statusfile.open("status_c.txt", std::fstream::out | std::fstream::app);
		statusfile << iter0->first.first;
		statusfile << "|";
		statusfile << ((int)iter0->first.second);

		statusfile << "|" << sequence_number;
		if(outcome)
		{
			statusfile << "|success\n";
			iter0->second.second -= score_improve_if_success +(gen()%5)*.01;
			std::cout << "Answer accepted!\n";
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			if(repeat == 3)
			{
				repeat = 0;
			}
		}
		else
		{
			statusfile << "|fail\n";
			iter0->second.second = std::min(
				(int)(iter0->second.second + score_deteriorate_if_fail+(gen()%5000)*.00001),
				std::max(
					(int)4,
					(int)(max_complexity* iter0->first.first.size()/6)
				)
			);

		}
		--repeat;
		if(repeat>0)
		{
			goto repeat_question;
		}
	}
}

bool chinese::questions::ask_1_chinese_char_easy(
	std::string question_number,
	std::string const& chinese_char,
	int breaks
) const
{
	if(chinese_char.empty())
	{
		std::cout << "Empty chinese character!\n";
		return false;
	}
	auto iter = chinese_char_to_index.find(chinese_char);
	if(iter == chinese_char_to_index.end())
	{
		std::cout << "Unknown chinese character: " << chinese_char << "\n";
		return false;
	}
	std::vector<datapoint const*> good_answers_remaining;
	std::vector<datapoint const*> good_answers_already_given;
	for(size_t index: iter->second)
	{
		good_answers_remaining.push_back(&loaded_data[index]);
	}
	while(good_answers_remaining.size())
	{
		std::stringstream question_to_ask;
		question_to_ask << question_number << " ";
		question_to_ask << "Character is " << chinese_char << ", what is " << to_string(DATATYPE_CHINESE);
		question_to_ask << "? The remaining good answer(s) is/are ";
		for(size_t i = 0; i < good_answers_remaining.size(); ++i)
		{
			if(i > 0) question_to_ask << ", ";
			question_to_ask << good_answers_remaining[i]->get(DATATYPE_ENGLISH);
		}
		question_to_ask << ".";
		question_to_ask << " Already given: ";
		for(size_t i = 0; i < good_answers_already_given.size(); ++i)
		{
			if(i > 0) question_to_ask << ", ";
			question_to_ask << good_answers_already_given[i]->get(DATATYPE_CHINESE);
		}
		std::string gave;
		std::stringstream chinese;
		for(size_t i = 0; i < good_answers_remaining.size(); ++i)
		{
			chinese << good_answers_remaining[i]->get(DATATYPE_CHINESE);
		}
		gave = m_input.do_input_chinese(question_to_ask.str(), true, breaks, chinese.str().c_str());
		bool found = false;
		for(size_t i = 0; i < good_answers_remaining.size(); ++i)
		{
			if(gave == good_answers_remaining[i]->get(DATATYPE_CHINESE))
			{
				std::cout << "Answer accepted!\n";
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
				good_answers_already_given.push_back(good_answers_remaining[i]);
				good_answers_remaining.erase(good_answers_remaining.begin() + i);
				found = true;
				break;
			}
		}
		if(!found)
		{
			std::cout << reset_color << "Wrong answer (" << gave << ")!\r\n";
			std::cout << "Correct would have been one of: ";
			for(size_t i = 0; i < good_answers_remaining.size(); ++i)
			{
				//std::cout << good_answers_remaining[i]->get(asked) << " ";
				std::cout << good_answers_remaining[i]->get(DATATYPE_CHINESE) << " ("
				          << good_answers_remaining[i]->get(DATATYPE_PINYIN) << ", "
				          << good_answers_remaining[i]->get(DATATYPE_ENGLISH) << ") ";
			}
			std::cout << "\r\n";
			//getch();
			system_pause();
			break;
		}
	}
	return good_answers_remaining.size() == 0;
}

bool chinese::questions::ask_all_chinese_chars_easy(
	int breaks
) const
{
	std::mt19937 gen(1337);//std::chrono::steady_clock::now().time_since_epoch().count());
	double score_improve_if_success = 1.04;
	double score_deteriorate_if_fail = 5.19;
	double max_complexity = 6.33;


	size_t sequence_number = 40;
	std::map<q_type_char_easy, std::pair<size_t, double>> recurrence_scores;
	recurrence_scores = load_status_file_char_easy(gen, sequence_number, score_improve_if_success, score_deteriorate_if_fail, max_complexity);

	character_statistics_screen_easy(recurrence_scores, breaks, sequence_number);

	while(1)
	{
		++sequence_number;

		std::vector<std::map<q_type_char_easy, std::pair<size_t, double>>::iterator> equal_chances;
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



		int repeat=3;
repeat_question:

		std::stringstream question_number;
		question_number.str("");
		question_number << iter0->first << "/" << chinese_char_to_index.size() << " "
			<< std::to_string(sequence_number) << " "
			<< std::to_string(iter0->second.first) << " "
			<< std::to_string(iter0->second.second);

		bool outcome = ask_1_chinese_char_easy(
			question_number.str(),
			iter0->first,
			breaks
		);
		iter0->second.first = sequence_number;
	// the meaning of the tokens
	// 0: chinese character
	// 1: asked datatype (0=chinese, 1=pinyin, 2=english)
	// 2: sequence number (when it was asked)
	// 3: outcome (success or fail)

		std::fstream statusfile;
		statusfile.open("status_c_e.txt", std::fstream::out | std::fstream::app);
		statusfile << iter0->first;

		statusfile << "|" << sequence_number;
		if(outcome)
		{
			statusfile << "|success\n";
			iter0->second.second -= score_improve_if_success +(gen()%5)*.01;
			std::cout << "Answer accepted!\n";
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			if(repeat == 3)
			{
				repeat = 0;
			}
		}
		else
		{
			statusfile << "|fail\n";
			iter0->second.second = std::min(
				(int)(iter0->second.second + score_deteriorate_if_fail+(gen()%5000)*.00001),
				std::max(
					(int)4,
					(int)(max_complexity* iter0->first.size()/6)
				)
			);
		}
		--repeat;
		if(repeat>0)
		{
			goto repeat_question;
		}
	}
}

std::map<chinese::questions::q_type_char_easy, std::pair<size_t, double>> chinese::questions::load_status_file_char_easy(
	std::mt19937& gen,
	size_t& sequence_number,
	double score_improve_if_success,
	double score_deteriorate_if_fail,
	double max_complexity
) const
{
	std::map<chinese::questions::q_type_char_easy, std::pair<size_t, double>> recurrence_scores;
	for(size_t q=0; q<loaded_data.size(); ++q)
	{
		std::vector<std::string> chinese_chars = m_input.split_into_chinese_characters(loaded_data[q].chinese);
		for(const std::string& chinese_char: chinese_chars)
		{
			recurrence_scores[q_type_char_easy(chinese_char)] = std::pair<size_t, double>(0, 0.0);
		}
	}

	// the meaning of the tokens
	// 0: chinese character
	// 1: sequence number (when it was asked)
	// 2: outcome (success or fail)
	sequence_number = 40;
	std::fstream statusfile;
	statusfile.open("status_c_e.txt", std::fstream::in);
	while(statusfile.good())
	{
		std::string line;
		getline(statusfile, line);
		std::vector<std::string> tokens = tokenize(line, '|');
		if(tokens.size() < 3)
		{
			continue;
		}
		std::string chinese_char = tokens[0];
		q_type_char_easy key = q_type_char_easy(chinese_char);
		auto iter = recurrence_scores.find(key);
		if(iter == recurrence_scores.end())
		{
			std::cout << "Unknown chinese character: " << chinese_char << "\n";
			continue;
		}
		iter->second.first = std::max(iter->second.first, (size_t)atoll(tokens[1].c_str()));
		sequence_number = std::max(sequence_number, iter->second.first);
		if(tokens[2] == "success")
		{
			iter->second.second -= score_improve_if_success + (gen()%5)*.01;
		}
		else if(tokens[2] == "fail")
		{
			iter->second.second = std::min(
				std::max(
					(int)4,
					(int)(max_complexity*chinese_char.size()/6)
				),
				int(iter->second.second + score_deteriorate_if_fail + (gen()%5000)*.00001)
			);
		}
		
	}
	return recurrence_scores;
}

void chinese::questions::character_statistics_screen_easy(
	std::map<chinese::questions::q_type_char_easy, std::pair<size_t, double>> const& recurrence_scores,
	int breaks,
	size_t sequence_number
) const
{
	std::cout << "Chinese character (easy) statistics:\r\n";
	size_t zero_score=0;
	std::map<int, int> positive_score;
	std::map<int, int> negative_score;
	long double total_score=0.0l;
	bool have_overloaded = false;
	size_t total_positive = 0;
	size_t total_negative = 0;
	PRINT(recurrence_scores.size());
	for(auto iter = recurrence_scores.begin(); iter != recurrence_scores.end(); ++iter)
	{
		double score = iter->second.second;
		if(false && score < -4)
		{
			have_overloaded = true;
			std::cout << "Overloaded score(" << score << "). ";
			std::cout << iter->first << "\r\n";
		}
		total_score += score;
		if(score < 0.0)
		{
			++negative_score[floor(score)];
			++total_negative;
		}
		else if(score == 0.0)
		{
			++zero_score;
		}
		else
		{
			++positive_score[ceil(score)];
			++total_positive;
		}
	}
	std::cout << "Total positive score: " << total_positive << "\r\n";
	std::cout << "Total negative score: " << total_negative << "\r\n";
	for(auto iter = negative_score.begin(); iter != negative_score.end(); ++iter)
	{
		std::cout << "Negative score: " << iter->first << " count: " << iter->second << "\r\n";
	}
	std::cout << "Zero score: " << zero_score << "\r\n";
	for(auto iter = positive_score.begin(); iter != positive_score.end(); ++iter)
	{
		std::cout << "Positive score: " << iter->first << " count: " << iter->second << "\r\n";
	}
	std::cout << "Total chinese characters: " << chinese_char_to_index.size() << "\r\n";
	std::cout << "Total score: " << total_score << "\r\n";
	std::cout << "Average score per character: "
	          << (total_score/recurrence_scores.size()) 
	          << "\r\n";
	std::cout << "Sequence number: " << sequence_number << "\r\n";
	if(cols() > 80 && breaks > 0)
	{
		std::cout << "Press any key to continue...\r\n";
		//getch();
		//move(0,0);
		//clear();
		//refresh();
	}
	else
	{
		std::cout << "Press enter to continue...\r\n";
		//std::cin.get();
		//move(0,0);
		//clear();
		//refresh();
	}
	std::cin.get();
}

void chinese::questions::init() const
{
	m_input.init();
}

void chinese::questions::close() const
{
	m_input.close();
}

void chinese::questions::system_pause() const
{
	m_input.system_pause();
}

int chinese::questions::cols() const
{
	return m_input.cols();
}
