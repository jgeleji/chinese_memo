#ifndef _UTILS_H_INCLUDED_
#define _UTILS_H_INCLUDED_

#include <random>
#include <string>

// The single RNG of the game: it decides which word is asked next and which
// characters are offered as wrong answers. It is seeded freshly for every
// run, so sessions never replay the same questions/dummies. Two escape
// hatches pin the seed for testing (to prove the console build and the
// Android build still run the identical logic, or to replay one session):
//   CM_SEED=<number> in the environment, or
//   a file "cm_seed" / "files/cm_seed" containing the number (Android has no
//   way to set environment variables)
std::mt19937& chinese_rng(void);

// Seed for the question-selection engine (score replay, which questions enter
// the next block, their order). This one must stay reproducible: the plain
// console app on the Termux phone seeds it with the constant 1337 at every
// start, so the Android/SDL port must do the same for the two to hand out the
// same questions in the same order from the same status file. A pinned seed
// (CM_SEED / cm_seed file) overrides it for test and replay purposes.
unsigned cm_selection_seed(void);

extern const char* reset_color;
extern const char* blue_background;
extern const char* red_background;
extern const char* green_foreground;
extern const char* yellow_foreground;
extern const char* purple_background;
extern const char* grey_background;
extern const char* green_background; 
extern const char* yellow_background;


std::string pad_to_three(int i);

bool replaceall(std::string& in, std::string from, std::string to);



#endif // _UTILS_H_INCLUDED_
