// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: sourcelist.cc,v 1.18 2001/02/20 07:03:17 jgg Exp $
/* ######################################################################

   List of Sources
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#ifdef __GNUG__
#pragma implementation "apt-pkg/sourcelist.h"
#endif

#include <apt-pkg/sourcelist.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/strutl.h>

#include <apti18n.h>

#include <fstream.h>
									/*}}}*/

// Global list of Item supported
static  pkgSourceList::Type *ItmList[10];
pkgSourceList::Type **pkgSourceList::Type::GlobalList = ItmList;
unsigned long pkgSourceList::Type::GlobalListLen = 0;

// Type::Type - Constructor						/*{{{*/
// ---------------------------------------------------------------------
/* Link this to the global list of items*/
pkgSourceList::Type::Type()
{
   ItmList[GlobalListLen] = this;
   GlobalListLen++;
}
									/*}}}*/
// Type::GetType - Get a specific meta for a given type			/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgSourceList::Type *pkgSourceList::Type::GetType(const char *Type)
{
   for (unsigned I = 0; I != GlobalListLen; I++)
      if (strcmp(GlobalList[I]->Name,Type) == 0)
	 return GlobalList[I];
   return 0;
}
									/*}}}*/
// Type::FixupURI - Normalize the URI and check it..			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgSourceList::Type::FixupURI(string &URI) const
{
   if (URI.empty() == true)
      return false;

   if (URI.find(':') == string::npos)
      return false;

   URI = SubstVar(URI,"$(ARCH)",_config->Find("APT::Architecture"));
   
   // Make sure that the URN is / postfixed
   if (URI[URI.size() - 1] != '/')
      URI += '/';
   
   return true;
}
									/*}}}*/
// Type::ParseLine - Parse a single line				/*{{{*/
// ---------------------------------------------------------------------
/* This is a generic one that is the 'usual' format for sources.list
   Weird types may override this. */
bool pkgSourceList::Type::ParseLine(vector<pkgIndexFile *> &List,
				    const char *Buffer,
				    unsigned long CurLine,
				    string File) const
{
   string URI;
   string Dist;
   string Section;   
   
   if (ParseQuoteWord(Buffer,URI) == false)
      return _error->Error(_("Malformed line %lu in source list %s (URI)"),CurLine,File.c_str());
   if (ParseQuoteWord(Buffer,Dist) == false)
      return _error->Error(_("Malformed line %lu in source list %s (dist)"),CurLine,File.c_str());
      
   if (FixupURI(URI) == false)
      return _error->Error(_("Malformed line %lu in source list %s (URI parse)"),CurLine,File.c_str());
   
   // Check for an absolute dists specification.
   if (Dist.empty() == false && Dist[Dist.size() - 1] == '/')
   {
      if (ParseQuoteWord(Buffer,Section) == true)
	 return _error->Error(_("Malformed line %lu in source list %s (Absolute dist)"),CurLine,File.c_str());
      Dist = SubstVar(Dist,"$(ARCH)",_config->Find("APT::Architecture"));
      return CreateItem(List,URI,Dist,Section);
   }
   
   // Grab the rest of the dists
   if (ParseQuoteWord(Buffer,Section) == false)
      return _error->Error(_("Malformed line %lu in source list %s (dist parse)"),CurLine,File.c_str());
   
   do
   {
      if (CreateItem(List,URI,Dist,Section) == false)
	 return false;
   }
   while (ParseQuoteWord(Buffer,Section) == true);
   
   return true;
}
									/*}}}*/

// SourceList::pkgSourceList - Constructors				/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgSourceList::pkgSourceList()
{
}

pkgSourceList::pkgSourceList(string File)
{
   Read(File);
}
									/*}}}*/
// SourceList::ReadMainList - Read the main source list from etc	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgSourceList::ReadMainList()
{
   return Read(_config->FindFile("Dir::Etc::sourcelist"));
}
									/*}}}*/
// SourceList::Read - Parse the sourcelist file				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgSourceList::Read(string File)
{
   // Open the stream for reading
   ifstream F(File.c_str(),ios::in | ios::nocreate);
   if (!F != 0)
      return _error->Errno("ifstream::ifstream",_("Opening %s"),File.c_str());
   
   List.erase(List.begin(),List.end());
   char Buffer[300];

   int CurLine = 0;
   while (F.eof() == false)
   {
      F.getline(Buffer,sizeof(Buffer));
      CurLine++;
      _strtabexpand(Buffer,sizeof(Buffer));
      
      
      char *I;
      for (I = Buffer; *I != 0 && *I != '#'; I++);
      *I = 0;
      
      const char *C = _strstrip(Buffer);
      
      // Comment or blank
      if (C[0] == '#' || C[0] == 0)
	 continue;
      	    
      // Grok it
      string LineType;
      if (ParseQuoteWord(C,LineType) == false)
	 return _error->Error(_("Malformed line %u in source list %s (type)"),CurLine,File.c_str());

      Type *Parse = Type::GetType(LineType.c_str());
      if (Parse == 0)
	 return _error->Error(_("Type '%s' is not known in on line %u in source list %s"),LineType.c_str(),CurLine,File.c_str());
      
      if (Parse->ParseLine(List,C,CurLine,File) == false)
	 return false;
   }
   return true;
}
									/*}}}*/
// SourceList::FindIndex - Get the index associated with a file		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgSourceList::FindIndex(pkgCache::PkgFileIterator File,
			      pkgIndexFile *&Found) const
{
   for (const_iterator I = List.begin(); I != List.end(); I++)
   {
      if ((*I)->FindInCache(*File.Cache()) == File)
      {
	 Found = *I;
	 return true;
      }
   }
   
   return false;
}
									/*}}}*/
// SourceList::GetIndexes - Load the index files into the downloader	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgSourceList::GetIndexes(pkgAcquire *Owner) const
{
   for (const_iterator I = List.begin(); I != List.end(); I++)
      if ((*I)->GetIndexes(Owner) == false)
	 return false;
   return true;
}
									/*}}}*/
