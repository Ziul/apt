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

float dice_coefficient(std::string string1, std::string string2)
{

        std::set<std::string> string1_bigrams;
        std::set<std::string> string2_bigrams;

        //base case
        if(string1.length() == 0 || string2.length() == 0)
        {
                return 0;
        }        

        for(unsigned int i = 0; i < (string1.length() - 1); i++) {      // extract character bigrams from string1
                string1_bigrams.insert(string1.substr(i, 2));
        }
        for(unsigned int i = 0; i < (string2.length() - 1); i++) {      // extract character bigrams from string2
                string2_bigrams.insert(string2.substr(i, 2));
        }

        int intersection = 0;
        
        // find the intersection between the two sets
        
        for(auto it = string2_bigrams.begin(); 
            it != string2_bigrams.end(); 
            it++)
        {    
                intersection += string1_bigrams.count((*it));
        }

        // calculate dice coefficient
        int total = string1_bigrams.size() + string2_bigrams.size();
        float dice = (float)(intersection * 2) / (float)total;

        return dice;
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
      {
         float distance = dice_coefficient(PkgName, CmdL.FileList[I + 1]);
	 if (distance >= 0.5 )
	 {
	    PkgsDone[P->ID] = true;
	    std::stringstream outs;
	    ListSingleVersion(CacheFile, records, V, outs, format);
	    outputVector.emplace_back(CacheFile, records, V, outs.str());
	 }
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
   for(auto k:outputVector)
      std::cout << k.formated_output() << std::endl;

   std::cerr << "time: " << time.currenttime() << std::endl;

   return true;
}
									/*}}}*/
