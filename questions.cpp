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
#include "utils.h"

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
		std::vector<std::string> english_tokens = tokenize(tokens[1], ';');
		for(size_t i=0; i<english_tokens.size(); ++i)
		{
			dp.english = tolower(english_tokens[i]);
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

// Words written with arabic digits ("3角2分") get their pinyin by deriving
// it from the chinese text, and that derivation passes single-byte
// characters through untouched: the expected pinyin is "3jiǎo2fēn", with
// literal digits. To answer the pinyin question the learner must type the
// number words themselves with their tones (san1 jiao3 er2 fen1), so before
// comparing, digits are canonized on the EXPECTED side only: a digit in the
// expected pinyin becomes its toned number word, unless it spells a tone
// mark inside an all-ascii syllable (legacy "ping2" entries keep their old
// behaviour). The typed answer is NOT canonized: a bare digit typed there
// stays a digit (see pinyin_convert_1syll) and never matches a number word,
// so digits do not answer their own question. For a chinese answer the
// grids offer the raw input (the 11th "pinyin echo" slot shows a typed
// digit as itself), so the typed chinese can contain the very digits of the
// expected text and compares equal untouched.
static const char* const number_word_pinyin[10] = {
	"líng", "yī", "èr", "sān", "sì", "wǔ", "liù", "qī", "bā", "jiǔ"
};

static bool is_ascii_letter(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static void canon_pinyin_numbers(std::string& s)
{
	std::string out;
	for(size_t i = 0; i < s.size(); ++i)
	{
		bool expand = true;
		if(s[i] >= '0' && s[i] <= '9')
		{
			// A digit is a tone mark only when the syllable in front of it
			// is written in tone-number style (all ascii letters, "ping2");
			// a syllable carrying a diacritic ("jiǎo") needs no tone digit,
			// so a digit after it is the standalone number word of
			// digit-containing texts ("3jiǎo2fēn")
			bool diacritic_syllable = false;
			size_t j = i;
			while(j > 0 && (is_ascii_letter(s[j-1]) || (unsigned char)s[j-1] >= 0x80))
			{
				if((unsigned char)s[j-1] >= 0x80) diacritic_syllable = true;
				--j;
			}
			expand = diacritic_syllable || j == i;   // digit with no letters before it: a number word
		}
		if(expand && s[i] >= '0' && s[i] <= '9')
			out += number_word_pinyin[s[i] - '0'];
		else
			out += s[i];
	}
	s.swap(out);
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
{	std::stringstream question_to_ask;
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
	std::string gave_c = nospace(tolower(gave));
	std::string asked_c = nospace(tolower(get(asked)));
	if(asked == chinese::questions::DATATYPE_PINYIN)
	{
		// expected side only: a typed digit must NOT become a number word
		canon_pinyin_numbers(asked_c);
	}
	return gave_c == asked_c;
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
	if(have_overloaded)	system_pause();
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
	system_pause();
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
					(double)global_total/recurrence_scores.size() - 1.5
				);
			global_total += change;
		}
		else if(tokens[6] == "fail")
		{
			typedef int min_type;
			//typedef double min_type;
			double before = iter->second.second;
			iter->second.second = 
				std::min(
					std::max(
						(min_type)4,
						(min_type)(max_complexity*tokens[0].size()/6)
					),
					(min_type)(iter->second.second + score_deteriorate_if_fail + (gen()%5000)*.00001)
				);
			double after = iter->second.second;
			global_total += after - before;
		}


	}
	return recurrence_scores;
}

bool chinese::questions::ask_all_until_fail_block10(
	std::function<double(int)> f,
	int breaks) const
{
	std::mt19937 gen(cm_selection_seed());   // reproducible: see utils.h
	double score_improve_if_success = 1.04;
	double score_deteriorate_if_fail = 5.19;
	double max_complexity = 6.33;

	size_t sequence_number = 0;

	auto recurrence_scores = load_status_file(gen, sequence_number, score_improve_if_success, score_deteriorate_if_fail, max_complexity);

	statistics_screen(recurrence_scores, breaks);

	for(auto iter = recurrence_scores.begin(); iter != recurrence_scores.end(); ++ iter)
	{
		q_type theq = iter->first;
		const datapoint* dp = &loaded_data[std::get<0>(theq)];
		double modifier = f(chinese::Input::count_unicode_code_points(dp->chinese));
		if(std::get<1>(theq) == DATATYPE_CHINESE || std::get<2>(theq) == DATATYPE_CHINESE)
			modifier += 1;
		iter->second.second += modifier;
	}

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
	std::shuffle(equal_chances.begin(), equal_chances.end(), gen);
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
			std::cout << "Answer accepted!\r\n";
			std::cout << "Btw\r\nCHI=" << loaded_data[std::get<0>(which_q)].get(DATATYPE_CHINESE);
			std::cout << ",\r\nPYN=" << loaded_data[std::get<0>(which_q)].get(DATATYPE_PINYIN);
			std::cout << ",\r\nENG=" << loaded_data[std::get<0>(which_q)].get(DATATYPE_ENGLISH) << "\r\n";
			m_input.show_for(900);
		}
		else
		{
			loc_seq = 0;
			std::cout << reset_color << "Wrong answer (" << gave << ")!\r\n";
			std::cout << "Correct would have been " << loaded_data[std::get<0>(which_q)].get(std::get<2>(which_q)) << "\r\n";
			std::cout << "Btw\r\nCHI=" << loaded_data[std::get<0>(which_q)].get(DATATYPE_CHINESE);
			std::cout << ",\r\nPYN=" << loaded_data[std::get<0>(which_q)].get(DATATYPE_PINYIN);
			std::cout << ",\r\nENG=" << loaded_data[std::get<0>(which_q)].get(DATATYPE_ENGLISH) << "\r\n";
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
			std::shuffle(equal_chances.begin(), equal_chances.end(), gen);
		}
	}
	return true;
}

// ---------------------------------------------------------------------------
// Gap-based scheduling, word-centric. One unit = one word entry; every word
// is asked through four forms in a fixed difficulty order:
//
//     form 0: chinese -> pinyin    (recall the reading)
//     form 1: chinese -> english   (recall the meaning)
//     form 2: pinyin  -> chinese    (write the characters from sound)
//     form 3: english -> chinese    (write the characters from meaning)
//
// (the two forms without chinese, english<->pinyin, are not asked: they only
// exercise pinyin spelling, which form 0 already covers.)
//
// The forms of one word count as the same question in a different shape, so
// they share all state: a word is asked at most one form per scheduling turn,
// which keeps the different forms evenly spaced (seeing the same word in
// another shape right away would be free points).
//
// Per word we keep:
//   interval       the demonstrated "completed distance": the maximum over
//                  completed rounds of the minimum of the three correct->
//                  correct gaps inside the round; 0 after a mistake (the
//                  point of a completed round: the whole chain must have
//                  been held that long)
//   last_correct_at  global question number of the newest correct answer of
//                  any form (-1: never answered correctly)
//   round_min      running minimum of the correct->correct gaps within the
//                  current round (-1: nothing recorded yet; gaps spanning a
//                  re-ask count, honestly shrinking the round)
//   form           which of the four forms to ask next
//   rounds_done    completed four-form rounds ("points")
//   last_seq       global number of when any form was last asked
//                  (-1: never asked, i.e. still a new word)
//
// load_gap_status_file reconstructs all of this from the status file (any
// mix of old 7-column rows, the intermediate 10-column rows and the current
// 12-column rows); ask_gap_mixed then asks ~70% from the seen pool (word
// with smallest interval first, oldest-asked on ties), ~30% brand-new words
// in increasing word length, and re-asks a word 5 questions after a mistake.
// Every answer appends one status line:
//   chinese|pinyin|english|provided|asked|seq|success|interval|
//   last_correct_at|round_min|form|rounds_done

static const chinese::questions::DATATYPE GAP_FORMS[4][2] =
{
	{ chinese::questions::DATATYPE_CHINESE, chinese::questions::DATATYPE_PINYIN },
	{ chinese::questions::DATATYPE_CHINESE, chinese::questions::DATATYPE_ENGLISH },
	{ chinese::questions::DATATYPE_PINYIN,  chinese::questions::DATATYPE_CHINESE },
	{ chinese::questions::DATATYPE_ENGLISH, chinese::questions::DATATYPE_CHINESE },
};

std::map<size_t, chinese::questions::gap_word> chinese::questions::load_gap_status_file(
	size_t& sequence_number) const
{
	std::map<size_t, gap_word> rec;
	for(size_t q=0; q<loaded_data.size(); ++q)
		rec[q] = gap_word{0, -1, -1, -1, 0, 0};

	sequence_number = 0;

	std::fstream statusfile;
	statusfile.open(statusfilename, std::fstream::in);
	while(statusfile.good())
	{
		std::string line;
		getline(statusfile, line);
		std::vector<std::string> tokens = tokenize(line, '|');
		if(tokens.size() < 6)
		{
			continue;
		}
		auto iter0 = question_index_finder.find(tokens[0]);
		if(iter0 == question_index_finder.end())
		{
			continue;
		}
		// older files (and the phone's rows) often leave pinyin/english empty:
		// fall back to the first variant registered for that word, exactly as
		// load_status_file does
		auto iter1 = iter0->second.find(tokens[1]);
		if(iter1 == iter0->second.end())
		{
			iter1 = iter0->second.begin();
		}
		auto iter2 = iter1->second.find(tokens[2]);
		if(iter2 == iter1->second.end())
		{
			iter2 = iter1->second.begin();
		}
		size_t q = iter2->second;
		gap_word& w = rec[q];
		long long seq = atoll(tokens[5].c_str());
		if(seq > w.last_seq) w.last_seq = seq;
		if((size_t)seq > sequence_number) sequence_number = (size_t)seq;

		if(tokens[6] == "success" && seq > w.last_correct_at)
		{
			w.last_correct_at = seq;
		}

		// current-format rows carry the whole state as it was right after this
		// answer: where present it wins over anything derived (so old rows and
		// new ones mix safely in either order). The intermediate 10-column
		// rows (the first gap-mode build) put a different value in column 10;
		// misreading it for one row is harmless, the next answer overwrites.
		if(tokens.size() >= 12 && !tokens[7].empty())
		{
			w.interval = atoll(tokens[7].c_str());
			w.last_correct_at = atoll(tokens[8].c_str());
			w.round_min = atoll(tokens[9].c_str());
			w.form = atoi(tokens[10].c_str());
			if(w.form < 0 || w.form > 3) w.form = 0;
			w.rounds_done = atoll(tokens[11].c_str());
		}
	}
	return rec;
}

// Headless driver hook (tools/qdriver.py): instead of driving the on-screen
// keyboard, the driver is told every question through CM_ASK_LOG and answers
// it through CM_DECIDE_FILE ("ok"/"bad" per line). The selection logic can
// then be verified without the SDL UI. Like the other CM_* hooks this is
// gated at runtime: no env vars set (any real session, incl. Android) means
// this code never does anything.
#include <cstdio>
#include <cstring>
#include <unistd.h>

static bool cm_gap_test_mode(void)
{
	return getenv("CM_ASK_LOG") != nullptr && getenv("CM_DECIDE_FILE") != nullptr;
}

typedef chinese::questions::DATATYPE cm_datatype;

static bool cm_gap_decide(
	std::string const& chinese_str,
	std::string const& pinyin_str,
	std::string const& english_str,
	cm_datatype provided,
	cm_datatype asked,
	size_t seq,
	std::string& gave)
{
	const char* asklog = getenv("CM_ASK_LOG");
	const char* decfile = getenv("CM_DECIDE_FILE");
	if(!asklog || !decfile) return false;   // no driver: fall back to real ask

	static FILE* logf = nullptr;
	if(!logf)
	{
		logf = fopen(asklog, "a");
		if(!logf) return false;
	}
	fprintf(logf, "ASK|%s|%s|%s|%d|%d|%zu\n",
		chinese_str.c_str(), pinyin_str.c_str(), english_str.c_str(),
		(int)provided, (int)asked, seq);
	fflush(logf);

	static long long decpos = 0;
	while(true)
	{
		FILE* f = fopen(decfile, "r");
		if(f)
		{
			fseek(f, decpos, SEEK_SET);
			char buf[64];
			if(fgets(buf, sizeof(buf), f))
			{
				decpos = ftell(f);
				fclose(f);
				if(strncmp(buf, "ok", 2) == 0)
				{
					if(asked == chinese::questions::DATATYPE_CHINESE) gave = chinese_str;
					else if(asked == chinese::questions::DATATYPE_PINYIN) gave = pinyin_str;
					else gave = english_str;
					return true;
				}
				gave = "\x01wrong";
				return false;
			}
			fclose(f);
		}
		usleep(2000);
	}
}

bool chinese::questions::ask_gap_mixed(int breaks) const
{
	const bool testmode = cm_gap_test_mode();
	const size_t REASK_GAP = 5;

	size_t sequence_number = 0;
	std::map<size_t, gap_word> rec = load_gap_status_file(sequence_number);
	const size_t words_total = rec.size();

	// new words in order of increasing complexity: shortest chinese word
	// first, then data order (the map walks in that order, so a stable sort
	// on the length is all that is needed)
	std::vector<size_t> unseen;
	size_t seen_total = 0;
	for(auto const& kv : rec)
	{
		if(kv.second.last_seq < 0)
			unseen.push_back(kv.first);
		else
			++seen_total;
	}
	std::stable_sort(unseen.begin(), unseen.end(), [this] (size_t a, size_t b)
	{
		size_t la = chinese::Input::count_unicode_code_points(loaded_data[a].chinese);
		size_t lb = chinese::Input::count_unicode_code_points(loaded_data[b].chinese);
		return la < lb;
	});

	// A mistake resets the interval to 0, but the interval-0 pool is huge and
	// asked oldest-first, so the word just missed would not come back for
	// ages. Keep a queue that re-asks a missed word (its same form) REASK_GAP
	// questions later; being asked again close after a miss is exactly what
	// makes a word stick.
	std::vector<std::pair<long long, size_t>> reaskq;   // (due-question, word)
	// words whose last attempt ended in a mistake (last_correct_at is older
	// than last_seq) and that were asked in the last 30 questions: the ones
	// still stinging when the previous session ended
	for(auto const& kv : rec)
		if(kv.second.last_seq >= 0
			&& kv.second.last_correct_at < kv.second.last_seq
			&& kv.second.last_seq + 30 >= (long long)sequence_number)
			reaskq.push_back(std::make_pair((long long)sequence_number, kv.first));

	size_t unseen_pos = 0;
	size_t session_total = 0, session_new = 0;

	std::cout << "Gap mode: " << seen_total << " seen, " << unseen.size()
		<< " new words. Forms per word: chi->pinyin, chi->english, pinyin->chi,"
		" english->chi.\r\n";
	if(testmode)
		fprintf(stderr, "gapmode words=%zu seen=%zu unseen=%zu reask=%zu seqmax=%zu\n",
			words_total, seen_total, unseen.size(), reaskq.size(), sequence_number);

	while(true)
	{
		++sequence_number;
		++session_total;

		bool take_new;
		if(unseen_pos >= unseen.size())
			take_new = false;
		else if(seen_total == 0)
			take_new = true;
		else
			take_new = 10*(session_new+1) <= 3*session_total;   // never above 30% new

		size_t which_q = 0;
		bool is_reask = false;
		bool have_pick = false;
		if(take_new)
		{
			which_q = unseen[unseen_pos++];
			++session_new;
			have_pick = true;
		}
		else
		{
			// a due re-ask (recently missed) goes before everything else in
			// the seen slot; earliest due first, word order on ties
			bool reask_found = false;
			size_t best_reask = 0;
			for(size_t i=0; i<reaskq.size(); ++i)
				if(reaskq[i].first <= (long long)sequence_number
					&& (!reask_found || reaskq[i].first < reaskq[best_reask].first))
				{
					reask_found = true;
					best_reask = i;
				}
			if(reask_found)
			{
				which_q = reaskq[best_reask].second;
				reaskq.erase(reaskq.begin() + best_reask);
				is_reask = true;
				have_pick = true;
			}
		}
		if(!have_pick)
		{
			// smallest demonstrated completed distance first; on ties the word
			// asked longest ago
			bool any = false;
			size_t best = 0;
			gap_word bestw{0,-1,-1,-1,0,0};
			for(auto const& kv : rec)
			{
				if(kv.second.last_seq < 0) continue;   // still new: not from here
				if(!any)
				{
					any = true;
					best = kv.first;
					bestw = kv.second;
					continue;
				}
				if(kv.second.interval < bestw.interval ||
					(kv.second.interval == bestw.interval && kv.second.last_seq < bestw.last_seq))
				{
					best = kv.first;
					bestw = kv.second;
				}
			}
			if(!any)
			{
				// nothing has ever been asked (fresh status file): the ratio
				// rule would starve, so take a new word regardless
				which_q = unseen[unseen_pos++];
				++session_new;
			}
			else
				which_q = best;
		}

		// whichever way it was picked, it is not waiting in the re-ask queue
		for(size_t i=0; i<reaskq.size(); ++i)
			if(reaskq[i].second == which_q)
			{
				reaskq.erase(reaskq.begin() + i);
				break;
			}

		gap_word& w = rec[which_q];
		DATATYPE provided = GAP_FORMS[w.form][0];
		DATATYPE asked = GAP_FORMS[w.form][1];
		datapoint const* current = &loaded_data[which_q];

		// same ambiguity guard as the other modes: another word that shows the
		// same provided text but wants a different answer would make this one
		// unanswerable, so name those to avoid
		std::vector<datapoint const*> others;
		std::string key = current->get(provided);
		std::unordered_map<std::string, std::unordered_set<int>>::const_iterator iter;
		std::unordered_map<std::string, std::unordered_set<int>> const* provided_overlaps;
		switch(provided)
		{
			case DATATYPE_PINYIN: provided_overlaps = &pinyin_overlaps; break;
			case DATATYPE_ENGLISH: provided_overlaps = &english_overlaps; break;
			default: provided_overlaps = &chinese_overlaps; break;
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

		// progress line: words answered correctly at least once (any form) out
		// of all words, then for every power of two how many words completed a
		// round distance of at least that ("g>=8:45" = 45 words that kept a
		// full four-form chain spaced 8 apart), then the question number
		size_t answered_once = 0;
		long long max_interval = 0;
		for(auto const& kv : rec)
		{
			if(kv.second.last_correct_at >= 0) ++answered_once;
			if(kv.second.interval > max_interval) max_interval = kv.second.interval;
		}
		std::string stats = std::to_string(answered_once) + "/" + std::to_string(words_total);
		for(long long p2=1; p2<=max_interval; p2*=2)
		{
			size_t cnt = 0;
			for(auto const& kv : rec)
				if(kv.second.interval >= p2) ++cnt;
			stats += " >=" + std::to_string(p2) + ":" + std::to_string(cnt);
		}
		stats += " q" + std::to_string(sequence_number);
		if(is_reask)
			stats += " re-ask";

		std::string gave;
		bool result = cm_gap_decide(
			current->get(DATATYPE_CHINESE),
			current->get(DATATYPE_PINYIN),
			current->get(DATATYPE_ENGLISH),
			provided, asked, sequence_number, gave);
		if(!result && gave.empty())   // no driver answering: ask for real
			result = current->ask(m_input, provided, asked, stats, others, gave, breaks);

		if(result)
		{
			// the gap from the previous correct answer (any form) counts
			// towards the round except for form 0, which opens a round
			long long gap = (w.last_correct_at >= 0)
				? (long long)sequence_number - w.last_correct_at : -1;
			if(w.form != 0 && gap >= 0)
				w.round_min = (w.round_min < 0) ? gap : std::min(w.round_min, gap);
			w.last_correct_at = (long long)sequence_number;
			if(w.form == 3)
			{
				// the full round is complete: its distance is the smallest of
				// its three gaps - the point of completing all four forms
				if(w.round_min >= 0)
				{
					if(w.round_min > w.interval) w.interval = w.round_min;
					++w.rounds_done;
				}
				w.round_min = -1;
			}
			w.form = (w.form + 1) % 4;
			std::cout << "Answer accepted!\r\n";
			if(!testmode)   // scripted runs would crawl with the display waits
			{
				std::cout << "Btw\r\nCHI=" << current->get(DATATYPE_CHINESE);
				std::cout << ",\r\nPYN=" << current->get(DATATYPE_PINYIN);
				std::cout << ",\r\nENG=" << current->get(DATATYPE_ENGLISH) << "\r\n";
				m_input.show_for(900);
			}
		}
		else
		{
			w.interval = 0;
			// the point of this whole queue: the miss comes back soon
			reaskq.push_back(std::make_pair((long long)sequence_number + REASK_GAP, which_q));
			std::cout << reset_color << "Wrong answer (" << gave << ")!\r\n";
			std::cout << "Correct would have been " << current->get(asked) << "\r\n";
			std::cout << "Btw\r\nCHI=" << current->get(DATATYPE_CHINESE);
			std::cout << ",\r\nPYN=" << current->get(DATATYPE_PINYIN);
			std::cout << ",\r\nENG=" << current->get(DATATYPE_ENGLISH) << "\r\n";
			if(!testmode)
				system_pause();
		}
		if(w.last_seq < 0)
			++seen_total;   // first time this word is asked: it joins the seen pool
		w.last_seq = (long long)sequence_number;

		{
			std::fstream statusfile;
			statusfile.open(statusfilename, std::fstream::out | std::fstream::app);
			statusfile << current->get(DATATYPE_CHINESE) << "|";
			statusfile << current->get(DATATYPE_PINYIN) << "|";
			statusfile << current->get(DATATYPE_ENGLISH) << "|";
			statusfile << ((int)provided) << "|" << ((int)asked);
			statusfile << "|" << sequence_number << "|";
			statusfile << (result ? "success" : "fail");
			statusfile << "|" << w.interval << "|" << w.last_correct_at
				<< "|" << w.round_min << "|" << w.form << "|" << w.rounds_done;
			statusfile << "\n";
			statusfile.close();
		}
	}
	return true;
}

bool chinese::questions::ask_all_until_fail(
	std::function<double(int)> f,
	int breaks) const
{
	std::mt19937 gen(cm_selection_seed());   // reproducible: see utils.h
	double score_improve_if_success = 1.04;
	double score_deteriorate_if_fail = 5.19;
	double max_complexity = 6.33;

	size_t sequence_number = 0;

	auto recurrence_scores = load_status_file(gen, sequence_number, score_improve_if_success, score_deteriorate_if_fail, max_complexity);

	statistics_screen(recurrence_scores, breaks);


	for(auto iter = recurrence_scores.begin(); iter != recurrence_scores.end(); ++ iter)
	{
		q_type theq = iter->first;
		const datapoint* dp = &loaded_data[std::get<0>(theq)];
		double modifier = f(chinese::Input::count_unicode_code_points(dp->chinese));
		iter->second.second += modifier;
	}
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
				reset_color
				+ std::to_string(q)
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
			m_input.show_for(900);
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
				m_input.show_for(900);
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
				m_input.show_for(900);
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
	system_pause();
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
	std::mt19937 gen(cm_selection_seed());   // reproducible: see utils.h
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
			m_input.show_for(900);
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
				m_input.show_for(900);
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
	std::mt19937 gen(cm_selection_seed());   // reproducible: see utils.h
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
			m_input.show_for(900);
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
	system_pause();
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
