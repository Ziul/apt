#ifndef APT_PRIVATE_SEARCH_H
#define APT_PRIVATE_SEARCH_H

#include <apt-pkg/macros.h>

class CommandLine;

APT_PUBLIC bool FullTextSearch(CommandLine &CmdL);
float dice_coefficient(std::string string1, std::string string2);

#endif
