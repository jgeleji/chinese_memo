//
// SDL entry wrapper for questions10 (block10 mode).
// main_questions10.cpp is #included below so that its plain main() becomes
// cm_real_main(); this file's SDL_main() is what SDL (Android: Java glue)
// actually calls. On Android we first chdir() into the app's internal storage
// (where the Java side copies union_q.txt, status1.txt, NotoSansSC.ttf and
// the data/ dictionary), because Input's constructor opens those files with
// relative paths before anything else runs.
//

#include <SDL.h>
#include <SDL_main.h>

#include <cstdio>
#include <cstdlib>

#ifdef __ANDROID__
#include <unistd.h>
#include <SDL_system.h>
#endif

int cm_real_main(int argc, const char* argv[]);

static void cm_setup(void)
{
#ifdef __ANDROID__
	const char* p = SDL_AndroidGetInternalStoragePath();
	if(p)
	{
		if(chdir(p) != 0) fprintf(stderr, "chdir(%s) failed\n", p);
	}
	else
	{
		fprintf(stderr, "SDL_AndroidGetInternalStoragePath failed\n");
	}
#endif
}

int SDL_main(int argc, char* argv[])
{
	cm_setup();
	return cm_real_main(argc, (const char**)argv);
}

// The desktop (non-Android) build needs a real main(): SDL's own SDLmain glue
// is only provided for some platforms, and on Android the entry point is
// SDL_main itself, resolved by SDLActivity through dlsym().
#ifdef main
#undef main
#endif
#ifndef __ANDROID__
int main(int argc, char* argv[])
{
	return SDL_main(argc, argv);
}
#endif

#undef main
#define main cm_real_main
#include "main_questions10.cpp"
#undef main
