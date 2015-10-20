#ifndef APT_PRIVATE_SEARCH_H
#define APT_PRIVATE_SEARCH_H

#include <apt-pkg/macros.h>

class CommandLine;

APT_PUBLIC bool FullTextSearch(CommandLine &CmdL);
int RK(std::string S, std::string K);
int levenshtein_distance(const std::string &s1, const std::string &s2);

#endif
