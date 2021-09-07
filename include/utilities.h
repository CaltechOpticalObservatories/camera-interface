/**
 * @file    utilties.h
 * @brief   some handy utilities to use anywhere
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#ifndef UTILITIES_H
#define UTILITIES_H

#include <iomanip>
#include <vector>
#include <sstream>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <unistd.h>

extern std::string zone;

unsigned int parse_val(const std::string& str);     //!< returns an unsigned int from a string

int Tokenize(const std::string& str, 
             std::vector<std::string>& tokens, 
             const std::string& delimiters);        //!< break a string into a vector

void chrrep(char *str, char oldchr, char newchr);   //!< replace one character within a string with a new character
void string_replace_char(std::string &str, const char *oldchar, const char *newchar);

long get_time( int &year, int &mon, int &mday, int &hour, int &min, int &sec, int &usec );

std::string get_timestamp();                        //!< return current time in formatted string "YYYY-MM-DDTHH:MM:SS.ssssss"
std::string get_system_date();                      //!< return current date in formatted string "YYYYMMDD"
std::string get_file_time();                        //!< return current time in formatted string "YYYYMMDDHHMMSS" used for filenames

double get_clock_time();

void timeout(float seconds=0, bool next_sec=true);

int compare_versions(std::string v1, std::string v2);

#endif
