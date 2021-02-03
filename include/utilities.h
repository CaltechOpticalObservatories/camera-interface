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

unsigned int parse_val(const std::string& str);     //!< returns an unsigned int from a string

int Tokenize(const std::string& str, 
             std::vector<std::string>& tokens, 
             const std::string& delimiters);        //!< break a string into a vector

void chrrep(char *str, char oldchr, char newchr);   //!< replace one character within a string with a new character
void string_replace_char(std::string &str, const char *oldchar, const char *newchar);

std::tm* get_timenow();                             //!< return tm pointer to current time

std::string get_system_time();                      //!< return current time in formatted string "YYYY-MM-DDTHH:MM:SS.ssssss"
std::string get_file_time();                        //!< return current time in formatted string "YYYYMMDDHHMMSS" used for filenames

double get_clock_time();

void timeout(float seconds=0, bool next_sec=true);

#endif
