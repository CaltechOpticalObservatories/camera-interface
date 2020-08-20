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

std::tm* get_timenow();                             //!< return tm pointer to current time

std::string get_time_string();                      //!< return current time in formatted string "YYYY-MM-DDTHH:MM:SS.ssssss"

double get_clock_time();

void timeout(float seconds=0, bool next_sec=true);

#endif
