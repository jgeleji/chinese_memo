#include <iostream>

#include "input.h"

int main()
{
	chinese::Input input;
	input.init();
	std::string inputted = input.do_input_chinese_notone("", false);
	input.close();

	std::cout << "inputted=" << inputted << std::endl;
	return 0;
}
