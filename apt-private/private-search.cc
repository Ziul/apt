// Includes								/*{{{*/
#include <config.h>

#include <apt-pkg/cachefile.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/policy.h>
#include <apt-pkg/progress.h>

#include <apt-private/private-cacheset.h>
#include <apt-private/private-search.h>
#include <apt-private/private-package-info.h>

#include <sstream>
#include <vector>
#include <algorithm>

									/*}}}*/

int RabinKarp(std::string StringInput, std::string Pattner) {
   #define RABINKARPHASH(a, b, h) ((((h) - (a)*d) << 1) + (b))
   std::transform(StringInput.begin(), StringInput.end(), StringInput.begin(), ::tolower);
   std::transform(Pattner.begin(), Pattner.end(), Pattner.begin(), ::tolower);
   const char *x = StringInput.c_str();
   int m = StringInput.length();
   const char *y = Pattner.c_str();
   int n = Pattner.length();

   int d, hx, hy, i, j;

   /* Preprocessing */
   /* computes d = 2^(m-1) with
      the left-shift operator */
   for (d = i = 1; i < m; ++i)
      d = (d<<1);

   for (hy = hx = i = 0; i < m; ++i) {
      hx = ((hx<<1) + x[i]);
      hy = ((hy<<1) + y[i]);
   }

   /* Searching */
   j = 0;
   while (j <= n-m) {
      if (hx == hy && memcmp(x, y + j, m) == 0)
	return j;
      hy = RABINKARPHASH(y[j], y[j + m], hy);
      ++j;
   }
   return -EXIT_FAILURE;
}

bool FullTextSearch(CommandLine &CmdL)					/*{{{*/
{
   pkgCacheFile CacheFile;
   pkgCache *Cache = CacheFile.GetPkgCache();
   pkgDepCache::Policy *Plcy = CacheFile.GetPolicy();
   if (unlikely(Cache == NULL || Plcy == NULL))
      return false;

   // Make sure there is at least one argument
   unsigned int const NumPatterns = CmdL.FileSize() -1;
   if (NumPatterns < 1)
      return _error->Error(_("You must give at least one search pattern"));

#define APT_FREE_PATTERNS() for (std::vector<regex_t>::iterator P = Patterns.begin(); \
      P != Patterns.end(); ++P) { regfree(&(*P)); }

   // Compile the regex pattern
   std::vector<regex_t> Patterns;
   if (_config->FindB("APT::Cache::UsingRegex",false))
      for (unsigned int I = 0; I != NumPatterns; ++I)
      {
	 regex_t pattern;
	 if (regcomp(&pattern, CmdL.FileList[I + 1], REG_EXTENDED | REG_ICASE | REG_NOSUB) != 0)
	 {
	    APT_FREE_PATTERNS();
	    return _error->Error("Regex compilation error");
	 }
	 Patterns.push_back(pattern);
      }

   bool const NamesOnly = _config->FindB("APT::Cache::NamesOnly", false);

   std::vector<PackageInfo> outputVector;

   LocalitySortedVersionSet bag;
   OpTextProgress progress(*_config);
   progress.OverallProgress(0, 100, 50,  _("Sorting"));
   GetLocalitySortedVersionSet(CacheFile, &bag, &progress);
   LocalitySortedVersionSet::iterator V = bag.begin();

   progress.OverallProgress(50, 100, 50,  _("Full Text Search"));
   progress.SubProgress(bag.size());
   pkgRecords records(CacheFile);

   std::string format = "${color:highlight}${Package}${color:neutral}/${Origin} ${Version} ${Architecture}${ }${apt:Status}\n";
   if (_config->FindB("APT::Cache::ShowFull",false) == false)
      format += "  ${Description}\n";
   else
      format += "  ${LongDescription}\n";

   int Done = 0;
   std::vector<bool> PkgsDone(Cache->Head().PackageCount, false);
   for ( ;V != bag.end(); ++V)
   {
      if (Done%500 == 0)
         progress.Progress(Done);
      ++Done;

      // we want to list each package only once
      pkgCache::PkgIterator const P = V.ParentPkg();
      if (PkgsDone[P->ID] == true)
	 continue;

      char const * const PkgName = P.Name();
      pkgCache::DescIterator Desc = V.TranslatedDescription();
      pkgRecords::Parser &parser = records.Lookup(Desc.FileList());
      std::string const LongDesc = parser.LongDesc();

      bool all_found = true;
      if (_config->FindB("APT::Cache::UsingRegex",false))
      {
	 for (std::vector<regex_t>::const_iterator pattern = Patterns.begin();
	       pattern != Patterns.end(); ++pattern)
	 {
	    if (regexec(&(*pattern), PkgName, 0, 0, 0) == 0)
	       continue;
	    else if (NamesOnly == false && regexec(&(*pattern), LongDesc.c_str(), 0, 0, 0) == 0)
	       continue;
	    // search patterns are AND, so one failing fails all
	    all_found = false;
	    break;
	 }
      }
      else
	 for (unsigned int I = 0; I != NumPatterns; ++I)
 	    if (((RabinKarp(CmdL.FileList[I + 1], P.Name()) < 0) ) and 
		( (NamesOnly == false) && (RabinKarp(CmdL.FileList[I + 1], LongDesc) < 0)))
 	    {
	       all_found = false;
 	       break;
 	    }

      if (all_found == true)
      {
	 PkgsDone[P->ID] = true;
	 std::stringstream outs;
	 ListSingleVersion(CacheFile, records, V, outs, format);
	 outputVector.emplace_back(CacheFile, records, V, outs.str());
      }	
   }
   switch(PackageInfo::getOrderByOption())
   {
      case PackageInfo::REVERSEALPHABETIC:
	 std::sort(outputVector.rbegin(), outputVector.rend(), OrderByAlphabetic);
	 break;
      case PackageInfo::STATUS:
	 std::sort(outputVector.begin(), outputVector.end(), OrderByStatus);
	 break;
      case PackageInfo::VERSION:
	 std::sort(outputVector.begin(), outputVector.end(), OrderByVersion);
	 break;
      default:
	 std::sort(outputVector.begin(), outputVector.end(), OrderByAlphabetic);
	 break;
   }
   if (_config->FindB("APT::Cache::UsingRegex",false))
      APT_FREE_PATTERNS();
   progress.Done();

   // output the sorted vector
   for(auto k:outputVector)
      std::cout << k.formated_output() << std::endl;

   return true;
}
									/*}}}*/
