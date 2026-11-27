#ifndef _INPUT_H_INCLUDED_
#define _INPUT_H_INCLUDED_

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <random>

extern const char* reset_color;
extern const char* blue_background;
extern const char* red_background;
extern const char* green_foreground;
extern const char* yellow_foreground;
extern const char* purple_foreground;
extern const char* grey_background;

namespace chinese
{

class Input
{
	private:
		std::unordered_map<std::string, std::unordered_set<std::string>> pinyin_to_chinese;
		std::unordered_map<std::string, std::unordered_set<std::string>> chinese_to_pinyin;
		std::unordered_map<std::string, std::string> chinese_to_preferred_pinyin;
		std::unordered_map<std::string, int> chinese_to_frequency;
		std::vector<std::string> all_chinese_chars;

		void load_extra_pinyins(std::string chinese);

		enum INPUT_STATE {
			INPUT_STATE_TYPE_PINYIN,
			INPUT_STATE_CHOOSE_NUMBER,
			INPUT_STATE_EXIT
		};

		void move_clear_refresh() const;
	public:
		void init() const;
		void close() const;
		void system_pause() const;
	// Show the screen as it currently is (including everything printed since
	// the last input) for ms milliseconds, then throw away any key/touch that
	// arrived meanwhile so it cannot leak into the next question. Used for the
	// "btw, here is the whole word" display after a correct answer.
	void show_for(int ms) const;
		int cols() const;

		void set_options(std::vector<std::string> const& options) const;

		std::vector<std::string> split_into_chinese_characters(std::string val) const;
		std::vector<std::string> get_some_random_chinese_chars(int num, std::mt19937& rnd) const;

		std::string do_input_chinese(std::string description="", bool shuffle=true, int breaks = 0, const char* expected=nullptr) const;
		std::string do_input_chinese_notone(std::string description="", bool shuffle=true, int breaks = 0, const char* expected=nullptr) const;
		std::string do_input_english(std::string description="") const;
		std::string do_input_pinyin(std::string description="") const;
		std::string do_input_1char_chinese(
			std::string const& top_row,
			INPUT_STATE& state_number,
			std::string const& description,
			bool shuffle=true,
			bool show_others=false,
			const char* expected=nullptr,
			int breaks = 0) const;
		std::string do_input_1char_chinese_notone(
			std::string const& top_row,
			INPUT_STATE& state_number,
			std::string const& description,
			bool shuffle=true,
			bool show_others=false,
			const char* expected=nullptr,
			int breaks = 0) const;
		std::string do_input_1char_english(std::string const& top_row, INPUT_STATE& state_number, std::string const& description) const;
		std::string do_input_1char_pinyin(std::string const& top_row, INPUT_STATE& state_number, std::string const& description) const;

		std::string do_input_inner_raw(
			std::string& ret,
			std::string& raw_pinyin,
			INPUT_STATE& state_number
		) const;

		std::string do_input_inner(
			std::string& ret,
			std::string& raw_pinyin,
			INPUT_STATE& state_number
		) const;

		// discard keystrokes that arrived but have not been read yet: used when
		// an answer is cancelled with "-" (the keys pressed around the cancel,
		// especially the second Enter of the accept sequence, must not leak
		// into the restarted answer)
		void flush_pending_input() const;

		// consume one Enter that is already sitting in the queue: a "-" cancel
		// fires on the "-" keystroke itself, and the Enter that the FIX cell
		// sends right behind it belongs to the cancel, not to the restarted
		// answer (without this the restarted answer starts out one Enter too
		// far, in its accept phase)
		void drop_queued_newline() const;

		std::vector<std::string> get_possibles(std::string const& pinyin) const;
		std::vector<std::string> get_possibles_fromflat(std::string const& pinyin) const;

		Input();
		std::unordered_set<std::string> const& get_chinese(std::string const& pinyin);
		std::string convert_chinese_to_pinyin(std::string chinese) const;

		static std::string pinyin_convert(std::string const& raw_pinyin);
		static std::string pinyin_convert_1syll(std::string const& raw_pinyin);

		static std::string pinyin_flatten_1syll(std::string const& raw_pinyin, int& tone);

		static const std::vector<std::pair<std::string, std::string>> replace;

		static int count_unicode_code_points(std::string const& str);
};

std::vector<std::string> const& current_options();

// Which input flow is currently running: the SDL on-screen key bar needs to
// know, because the CLEAR (erase-the-whole-typed-word) cell only makes sense
// in the chinese-character flow, while the FIX (discard-typed-answer) cell
// only makes sense in the free-typed english/pinyin flows.
enum INPUT_FLOW { INPUT_FLOW_NONE = 0, INPUT_FLOW_CHINESE = 1, INPUT_FLOW_FREE = 2 };
void set_input_flow(int flow);   // backend hook (ncurses: no-op)

}

#endif // _INPUT_H_INCLUDED_
