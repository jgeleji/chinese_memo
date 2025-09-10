#include <iostream>

#include "input.h"

int main()
{
	chinese::Input input;
	//input.write_dl_script();
	input.init();
	std::string inputted = input.do_input();

	input.close();
	std::cout << "inputted=" << inputted << std::endl;
	return 0;
}
