#ifndef _UTILS_H_INCLUDED_
#define _UTILS_H_INCLUDED_

#include <string>

extern const char* reset_color;
extern const char* blue_background;
extern const char* red_background;
extern const char* green_foreground;
extern const char* yellow_foreground;
extern const char* purple_background;
extern const char* grey_background;


std::string pad_to_three(int i);

bool replaceall(std::string& in, std::string from, std::string to);



#endif // _UTILS_H_INCLUDED_
