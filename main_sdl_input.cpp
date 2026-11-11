//
// SDL entry wrapper for the plain "input" screen (pinyin -> numbered character
// choices). Used as a quick development/test target for the touch buttons and
// the font rendering: it exercises do_input_chinese() without a question set.
//

#include <SDL.h>
#include <SDL_main.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#ifdef __ANDROID__
#include <unistd.h>
#include <SDL_system.h>
#endif

int cm_real_main(void);

static void cm_setup(void)
{
#ifdef __ANDROID__
	const char* p = SDL_AndroidGetInternalStoragePath();
	if(p)
	{
		if(chdir(p) != 0) fprintf(stderr, "chdir(%s) failed\n", p);
	}
#endif
}

int SDL_main(int argc, char* argv[])
{
	(void)argc;
	(void)argv;
	cm_setup();
	return cm_real_main();
}

#ifdef main
#undef main
#endif
#ifndef __ANDROID__
int main(int argc, char* argv[])
{
	(void)argc;
	(void)argv;
	return SDL_main(0, NULL);
}
#endif

#undef main
#define main cm_real_main
#include "main_input.cpp"
#undef main
