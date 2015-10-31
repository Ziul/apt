#ifndef APT_PRIVATE_SEARCH_H
#define APT_PRIVATE_SEARCH_H

#include <apt-pkg/macros.h>

class CommandLine;

APT_PUBLIC bool FullTextSearch(CommandLine &CmdL);
bool identify_regex(std::vector<std::string> input);
int RabinKarp(std::string StringInput, std::string Pattern);
int levenshtein_distance(const std::string &s1, const std::string &s2);

#endif
