#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

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

		void load_extra_pinyins(std::string chinese);

		enum INPUT_STATE {
			INPUT_STATE_TYPE_PINYIN,
			INPUT_STATE_CHOOSE_NUMBER,
			INPUT_STATE_EXIT
		};
	public:

		std::string do_input_chinese(std::string description="", bool shuffle=true) const;
		std::string do_input_english(std::string description="") const;
		std::string do_input_pinyin(std::string description="") const;
		std::string do_input_1char_chinese(std::string const& top_row, INPUT_STATE& state_number, std::string const& description, bool shuffle=true) const;
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

		std::vector<std::string> get_possibles(std::string const& pinyin) const;
		std::vector<std::string> get_possibles_fromflat(std::string const& pinyin) const;

		Input();
		std::unordered_set<std::string> const& get_chinese(std::string const& pinyin);
		std::string convert_chinese_to_pinyin(std::string chinese) const;

		static std::string pinyin_convert(std::string const& raw_pinyin);
		static std::string pinyin_convert_1syll(std::string const& raw_pinyin);

		static std::string pinyin_flatten_1syll(std::string const& raw_pinyin, int& tone);

		static const std::vector<std::pair<std::string, std::string>> replace;
};

}
