// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: gzip.cc,v 1.17.2.1 2004/01/16 18:58:50 mdz Exp $
/* ######################################################################

   GZip method - Take a file URI in and decompress it into the target 
   file.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/configuration.h>
#include <apt-pkg/acquire-method.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/aptconfiguration.h>
#include "aptmethod.h"

#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <string>
#include <vector>

#include <apti18n.h>
									/*}}}*/

class GzipMethod : public aptMethod
{
   std::string const Prog;
   virtual bool Fetch(FetchItem *Itm) APT_OVERRIDE;

   public:

   explicit GzipMethod(std::string const &pProg) : aptMethod(pProg.c_str(),"1.1",SingleInstance | SendConfig), Prog(pProg) {};
};

// GzipMethod::Fetch - Decompress the passed URI			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool GzipMethod::Fetch(FetchItem *Itm)
{
   URI Get = Itm->Uri;
   std::string Path = Get.Host + Get.Path; // To account for relative paths
   
   FetchResult Res;
   Res.Filename = Itm->DestFile;
   URIStart(Res);

   std::vector<APT::Configuration::Compressor> const compressors = APT::Configuration::getCompressors();
   std::vector<APT::Configuration::Compressor>::const_iterator compressor = compressors.begin();
   for (; compressor != compressors.end(); ++compressor)
      if (compressor->Name == Prog)
	 break;
   if (compressor == compressors.end())
      return _error->Error("Extraction of file %s requires unknown compressor %s", Path.c_str(), Prog.c_str());

   // Open the source and destination files
   FileFd From;
   if (_config->FindB("Method::Compress", false) == false)
   {
      From.Open(Path, FileFd::ReadOnly, *compressor);
      if(From.FileSize() == 0)
	 return _error->Error(_("Empty files can't be valid archives"));
   }
   else
      From.Open(Path, FileFd::ReadOnly);
   if (From.IsOpen() == false || From.Failed() == true)
      return false;

   FileFd To;
   if (Itm->DestFile != "/dev/null")
   {
      if (_config->FindB("Method::Compress", false) == false)
	 To.Open(Itm->DestFile, FileFd::WriteAtomic);
      else
	 To.Open(Itm->DestFile, FileFd::WriteOnly | FileFd::Create | FileFd::Empty, *compressor);

      if (To.IsOpen() == false || To.Failed() == true)
	 return false;
      To.EraseOnFailure();
   }


   // Read data from source, generate checksums and write
   Hashes Hash(Itm->ExpectedHashes);
   bool Failed = false;
   Res.Size = 0;
   while (1) 
   {
      unsigned char Buffer[4*1024];
      unsigned long long Count = 0;
      
      if (!From.Read(Buffer,sizeof(Buffer),&Count))
      {
	 if (To.IsOpen())
	    To.OpFail();
	 return false;
      }
      if (Count == 0)
	 break;
      Res.Size += Count;

      Hash.Add(Buffer,Count);
      if (To.IsOpen() && To.Write(Buffer,Count) == false)
      {
	 Failed = true;
	 break;
      }      
   }
   
   From.Close();
   To.Close();

   if (Failed == true)
      return false;

   // Transfer the modification times
   if (Itm->DestFile != "/dev/null")
   {
      struct stat Buf;
      if (stat(Path.c_str(),&Buf) != 0)
	 return _error->Errno("stat",_("Failed to stat"));

      struct timeval times[2];
      times[0].tv_sec = Buf.st_atime;
      Res.LastModified = times[1].tv_sec = Buf.st_mtime;
      times[0].tv_usec = times[1].tv_usec = 0;
      if (utimes(Itm->DestFile.c_str(), times) != 0)
	 return _error->Errno("utimes",_("Failed to set modification time"));
   }

   // Return a Done response
   Res.TakeHashes(Hash);

   URIDone(Res);
   return true;
}
									/*}}}*/

int main(int, char *argv[])
{
   setlocale(LC_ALL, "");

   GzipMethod Mth(flNotDir(argv[0]));
   return Mth.Run();
}
