// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: cachedb.cc,v 1.5 2002/11/22 18:02:08 doogie Exp $
/* ######################################################################

   CacheDB
   
   Simple uniform interface to a cache database.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#ifdef __GNUG__
#pragma implementation "cachedb.h"
#endif

#include "cachedb.h"

#include <apt-pkg/error.h>
#include <apt-pkg/md5.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/configuration.h>
    
#include <netinet/in.h>       // htonl, etc
									/*}}}*/

// CacheDB::ReadyDB - Ready the DB2					/*{{{*/
// ---------------------------------------------------------------------
/* This opens the DB2 file for caching package information */
bool CacheDB::ReadyDB(string DB)
{
   ReadOnly = _config->FindB("APT::FTPArchive::ReadOnlyDB",false);
   
   // Close the old DB
   if (Dbp != 0) 
      Dbp->close(Dbp,0);
   
   /* Check if the DB was disabled while running and deal with a 
      corrupted DB */
   if (DBFailed() == true)
   {
      _error->Warning("DB was corrupted, file renamed to %s.old",DBFile.c_str());
      rename(DBFile.c_str(),(DBFile+".old").c_str());
   }
   
   DBLoaded = false;
   Dbp = 0;
   DBFile = string();
   
   if (DB.empty())
      return true;
   
   if ((errno = db_open(DB.c_str(),DB_HASH,
                        (ReadOnly?DB_RDONLY:DB_CREATE),
                        0644,0,0,&Dbp)) != 0)
   {
      Dbp = 0;
      return _error->Errno("db_open","Unable to open DB2 file %s",DB.c_str());
   }
   
   DBFile = DB;
   DBLoaded = true;
   return true;
}
									/*}}}*/
// CacheDB::SetFile - Select a file to be working with			/*{{{*/
// ---------------------------------------------------------------------
/* All future actions will be performed against this file */
bool CacheDB::SetFile(string FileName,struct stat St,FileFd *Fd)
{
   delete DebFile;
   DebFile = 0;
   this->FileName = FileName;
   this->Fd = Fd;
   this->FileStat = St;
   FileStat = St;   
   memset(&CurStat,0,sizeof(CurStat));
   
   Stats.Bytes += St.st_size;
   Stats.Packages++;
   
   if (DBLoaded == false)
      return true;

   InitQuery("st");
   
   // Ensure alignment of the returned structure
   Data.data = &CurStat;
   Data.ulen = sizeof(CurStat);
   Data.flags = DB_DBT_USERMEM;
   // Lookup the stat info and confirm the file is unchanged
   if (Get() == true)
   {
      if (CurStat.mtime != htonl(St.st_mtime))
      {
	 CurStat.mtime = htonl(St.st_mtime);
	 CurStat.Flags = 0;
	 _error->Warning("File date has changed %s",FileName.c_str());
      }      
   }      
   else
   {
      CurStat.mtime = htonl(St.st_mtime);
      CurStat.Flags = 0;
   }   
   CurStat.Flags = ntohl(CurStat.Flags);
   OldStat = CurStat;
   return true;
}
									/*}}}*/
// CacheDB::LoadControl - Load Control information			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool CacheDB::LoadControl()
{
   // Try to read the control information out of the DB.
   if ((CurStat.Flags & FlControl) == FlControl)
   {
      // Lookup the control information
      InitQuery("cl");
      if (Get() == true && Control.TakeControl(Data.data,Data.size) == true)
	    return true;
      CurStat.Flags &= ~FlControl;
   }
   
   // Create a deb instance to read the archive
   if (DebFile == 0)
   {
      DebFile = new debDebFile(*Fd);
      if (_error->PendingError() == true)
	 return false;
   }
   
   Stats.Misses++;
   if (Control.Read(*DebFile) == false)
      return false;

   if (Control.Control == 0)
      return _error->Error("Archive has no control record");
   
   // Write back the control information
   InitQuery("cl");
   if (Put(Control.Control,Control.Length) == true)
      CurStat.Flags |= FlControl;
   return true;
}
									/*}}}*/
// CacheDB::LoadContents - Load the File Listing			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool CacheDB::LoadContents(bool GenOnly)
{
   // Try to read the control information out of the DB.
   if ((CurStat.Flags & FlContents) == FlContents)
   {
      if (GenOnly == true)
	 return true;
      
      // Lookup the contents information
      InitQuery("cn");
      if (Get() == true)
      {
	 if (Contents.TakeContents(Data.data,Data.size) == true)
	    return true;
      }
      
      CurStat.Flags &= ~FlContents;
   }
   
   // Create a deb instance to read the archive
   if (DebFile == 0)
   {
      DebFile = new debDebFile(*Fd);
      if (_error->PendingError() == true)
	 return false;
   }

   if (Contents.Read(*DebFile) == false)
      return false;	    
   
   // Write back the control information
   InitQuery("cn");
   if (Put(Contents.Data,Contents.CurSize) == true)
      CurStat.Flags |= FlContents;
   return true;
}
									/*}}}*/
// CacheDB::GetMD5 - Get the MD5 hash					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool CacheDB::GetMD5(string &MD5Res,bool GenOnly)
{
   // Try to read the control information out of the DB.
   if ((CurStat.Flags & FlMD5) == FlMD5)
   {
      if (GenOnly == true)
	 return true;
      
      InitQuery("m5");
      if (Get() == true)
      {
	 MD5Res = string((char *)Data.data,Data.size);
	 return true;
      }
      CurStat.Flags &= ~FlMD5;
   }
   
   Stats.MD5Bytes += FileStat.st_size;
	 
   MD5Summation MD5;
   if (Fd->Seek(0) == false || MD5.AddFD(Fd->Fd(),FileStat.st_size) == false)
      return false;
   
   MD5Res = MD5.Result();
   InitQuery("m5");
   if (Put(MD5Res.c_str(),MD5Res.length()) == true)
      CurStat.Flags |= FlMD5;
   return true;
}
									/*}}}*/
// CacheDB::Finish - Write back the cache structure			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool CacheDB::Finish()
{
   // Optimize away some writes.
   if (CurStat.Flags == OldStat.Flags &&
       CurStat.mtime == OldStat.mtime)
      return true;
   
   // Write the stat information
   CurStat.Flags = htonl(CurStat.Flags);
   InitQuery("st");
   Put(&CurStat,sizeof(CurStat));
   CurStat.Flags = ntohl(CurStat.Flags);
   return true;
}
									/*}}}*/
// CacheDB::Clean - Clean the Database					/*{{{*/
// ---------------------------------------------------------------------
/* Tidy the database by removing files that no longer exist at all. */
bool CacheDB::Clean()
{
   if (DBLoaded == false)
      return true;

   /* I'm not sure what VERSION_MINOR should be here.. 2.4.14 certainly
      needs the lower one and 2.7.7 needs the upper.. */
#if DB_VERSION_MAJOR >= 2 && DB_VERSION_MINOR >= 7
   DBC *Cursor;
   if ((errno = Dbp->cursor(Dbp,0,&Cursor,0)) != 0)
      return _error->Error("Unable to get a cursor");
#else
   DBC *Cursor;
   if ((errno = Dbp->cursor(Dbp,0,&Cursor)) != 0)
      return _error->Error("Unable to get a cursor");
#endif
   
   DBT Key;
   DBT Data;
   memset(&Key,0,sizeof(Key));
   memset(&Data,0,sizeof(Data));
   while ((errno = Cursor->c_get(Cursor,&Key,&Data,DB_NEXT)) == 0)
   {
      const char *Colon = (char *)Key.data;
      for (; Colon != (char *)Key.data+Key.size && *Colon != ':'; Colon++);
      if ((char *)Key.data+Key.size - Colon > 2)
      {
	 if (stringcmp((char *)Key.data,Colon,"st") == 0 ||
	     stringcmp((char *)Key.data,Colon,"cn") == 0 ||
	     stringcmp((char *)Key.data,Colon,"m5") == 0 ||
	     stringcmp((char *)Key.data,Colon,"cl") == 0)
	 {
	    if (FileExists(string(Colon+1,(const char *)Key.data+Key.size)) == true)
		continue;	     
	 }
      }
      
      Cursor->c_del(Cursor,0);
   }

   return true;
}
									/*}}}*/
