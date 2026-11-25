#ifndef _QUESTIONS_H_INCLUDED_
#define _QUESTIONS_H_INCLUDED_

#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <functional>
#include "input.h"

namespace chinese
{

class questions
{
	public:
		void init() const;
		void close() const;
		void system_pause() const;
		int cols() const;

		enum DATATYPE
		{
			DATATYPE_CHINESE,
			DATATYPE_ENGLISH,
			DATATYPE_PINYIN
		};
		
		static std::string to_string(DATATYPE val);

		questions(std::string statusfilename);
		void load_file_hsk(std::string const& filename);
		void load_file(std::string const& filename);
	
		bool ask_all_until_fail(
			std::function<double(int)> f = [](int) -> double { return 0.0; },
			int breaks = 0) const;
		bool ask_all_until_fail_block10(
			std::function<double(int)> f = [](int) -> double { return 0.0; },
			int breaks = 0) const;

		// Gap-based scheduling (the mode questions10 uses), word-centric: one
		// unit is one word, asked through four forms in a fixed order
		// (chi->pinyin, chi->english, pinyin->chi, english->chi); ~70% of
		// the questions come from the already-seen word pool (asked in
		// increasing demonstrated completed distance, oldest-asked first on
		// ties), ~30% are still-new words taken in order of increasing word
		// length; a word answered wrong is re-asked in the same form a few
		// questions later (and recent unresolved misses are re-asked early
		// even in a later run). One status line is written per question,
		// with the per-word record appended as five extra columns (old
		// 7-column files load unchanged). See gap_word below.
		bool ask_gap_mixed(int breaks = 0) const;
		void populate_chinese_char_to_index();

		bool ask_1_chinese_char(
			std::string question_number,
			std::string const& chinese_char,
			DATATYPE asked,
			int breaks = 0
		) const;

		bool ask_all_chinese_chars(
			int breaks = 0
		) const;

		bool ask_1_chinese_char_easy(
			std::string question_number,
			std::string const& chinese_char,
			int breaks = 0
		) const;

		bool ask_all_chinese_chars_easy(
			int breaks = 0
		) const;

	private:
		class datapoint
		{
			public:
				std::string const& get(DATATYPE which) const;

				std::string chinese;
				std::string english;
				std::string pinyin;

				bool ask(
					chinese::Input input,
					DATATYPE provided,
					DATATYPE asked,
					std::string const& question_number,
					std::vector<datapoint const*> const& others,
					std::string& gave,
					int breaks = 0
				) const;
		};

		// normal quesions
		typedef std::tuple<size_t, DATATYPE, DATATYPE> q_type;

		// Gap-based scheduling (ask_gap_mixed). For every word (index into
		// loaded_data) we keep, shared by all its four forms:
		//   interval         the demonstrated "completed distance": the
		//                    maximum over completed four-form rounds of the
		//                    minimum of the three correct->correct gaps in
		//                    the round; 0 after a mistake
		//   last_correct_at  global question number of the newest correct
		//                    answer of any form (-1: never correct)
		//   round_min        running minimum of the correct->correct gaps
		//                    within the current round (-1: none recorded)
		//   last_seq         global number of when any form was last asked
		//                    (-1: never asked, i.e. still a new word)
		//   form             which of the four forms to ask next
		//   rounds_done      completed four-form rounds ("points")
		struct gap_word
		{
			long long interval;
			long long last_correct_at;
			long long round_min;
			long long last_seq;
			int form;
			long long rounds_done;
		};

		std::map<size_t, gap_word> load_gap_status_file(size_t& sequence_number) const;

		std::map<q_type, std::pair<size_t, double>> load_status_file(
			std::mt19937& gen,
			size_t& sequence_number,
			double score_improve_if_success = 1.04,
			double score_deteriorate_if_fail = 5.19,
			double max_complexity = 6.33
		) const;

		typedef std::pair<std::string, DATATYPE> q_type_char;

		void statistics_screen(
			std::map<q_type, std::pair<size_t, double>> const& recurrence_scores,
			int breaks
		) const;

		// character questions
		void character_statistics_screen(
			std::map<q_type_char, std::pair<size_t, double>> const& recurrence_scores,
			int breaks,
			size_t sequence_number
		) const;

		std::map<q_type_char, std::pair<size_t, double>> load_status_file_char(
			std::mt19937& gen,
			size_t& sequence_number,
			double score_improve_if_success = 1.04,
			double score_deteriorate_if_fail = 5.19,
			double max_complexity = 6.33
		) const;

		// easy character questions
		typedef std::string q_type_char_easy;

		std::map<q_type_char_easy, std::pair<size_t, double>> load_status_file_char_easy(
			std::mt19937& gen,
			size_t& sequence_number,
			double score_improve_if_success = 1.04,
			double score_deteriorate_if_fail = 5.19,
			double max_complexity = 6.33
		) const;

		void character_statistics_screen_easy(
			std::map<q_type_char_easy, std::pair<size_t, double>> const& recurrence_scores,
			int breaks,
			size_t sequence_number
		) const;

		// data
		std::unordered_map<std::string, std::unordered_set<int>> chinese_char_to_index;

		std::vector<datapoint> loaded_data;
		std::unordered_map<std::string, std::unordered_set<int>> pinyin_overlaps;
		std::unordered_map<std::string, std::unordered_set<int>> english_overlaps;
		std::unordered_map<std::string, std::unordered_set<int>> chinese_overlaps;
		std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<std::string, int>>> question_index_finder;
		chinese::Input m_input;
		std::string statusfilename;
};

}

#endif // _QUESTIONS_H_INCLUDED_
