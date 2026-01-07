
#include <iostream>

#include "input.h"
#include "questions.h"

int main(int argc, const char* argv[])
{
	chinese::questions ques("status1.txt");
	ques.load_file("union_q.txt");
	ques.populate_chinese_char_to_index();


	ques.init();

	ques.ask_all_chinese_chars_easy(std::max(0, ques.cols()/7));

	ques.close();
	return 0;
}
