#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include "input.h"

class questions
{
	public:

		enum DATATYPE
		{
			DATATYPE_CHINESE,
			DATATYPE_ENGLISH,
			DATATYPE_PINYIN
		};
		
		static std::string to_string(DATATYPE val);

		questions();
		void load_file(std::string const& filename);
	
		bool ask_1(
			size_t which,
			DATATYPE provided,
			DATATYPE asked
		) const;

		bool ask_all_until_fail() const;

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
					std::string& gave
				) const;
		};

		std::vector<datapoint> loaded_data;
		std::unordered_map<std::string, std::unordered_set<int>> pinyin_overlaps;
		chinese::Input m_input;
};
