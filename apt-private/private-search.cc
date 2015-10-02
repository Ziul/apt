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
#include <apt-private/timer.h>

									/*}}}*/

#define REHASH(a, b, h) ((((h) - (a)*d) << 1) + (b))
int RK(std::string S, std::string K) {
   const char *x = S.c_str();
   int m = S.length();
   const char *y = K.c_str();
   int n = K.length();

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
      hy = REHASH(y[j], y[j + m], hy);
      ++j;
   }
   return -1;

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


   std::vector<PackageInfo> outputVector;
   timer time;

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
   time.begin();
   for ( ;V != bag.end(); ++V)
   {
      if (Done%500 == 0)
         progress.Progress(Done);
      ++Done;

      // we want to list each package only once
      pkgCache::PkgIterator const P = V.ParentPkg();
      if (PkgsDone[P->ID] == true)
	 continue;

      std::string PkgName = P.Name();
      pkgCache::DescIterator Desc = V.TranslatedDescription();
      pkgRecords::Parser &parser = records.Lookup(Desc.FileList());
      std::string const LongDesc = parser.LongDesc();

      for (unsigned int I = 0; I != NumPatterns; ++I)
	if ((RK(PkgName, CmdL.FileList[I + 1]) ) >=0)
	{
	 PkgsDone[P->ID] = true;
	 std::stringstream outs;
	 ListSingleVersion(CacheFile, records, V, outs, format);
	 outputVector.emplace_back(CacheFile, records, V, outs.str());
	}
   }
   time.end();
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
   progress.Done();

   // output the sorted vector
   // for(auto k:outputVector)
   //    std::cout << k.formated_output() << std::endl;

   std::cerr << "time: " << time.currenttime() << std::endl;

   return true;
}
									/*}}}*/
