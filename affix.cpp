// This file is part of The New Aspell
// Copyright (C) 2002 by Kevin Atkinson under the GNU LGPL
// license version 2.0 or 2.1.  You should have received a copy of the
// LGPL license along with this library if you did not you can find it
// at http://www.gnu.org/.
//
// Copyright 2002 Kevin B. Hendricks, Stratford, Ontario, Canada And
// Contributors.  All rights reserved. See the file affix.license for
// details.

#include <cstdlib>
#include <cstring>
#include <cstdio>

#include "affix.hpp"
#include "affentry.hpp"

using namespace std;

namespace aspeller {


// First some base level utility routines
void   mychomp(char * s);                          // remove end of line char(s)
char * mystrdup(const char * s);                   // duplicate string
char * myrevstrdup(const char * s);                // duplicate reverse of string
char * mystrsep(char ** sptr, const char delim);   // parse into tokens with char delimiter
int    isSubset(const char * s1, const char * s2); // is affix s1 is a "subset" affix s2


AffixMgr::AffixMgr(const char * affpath, BasicWordSet * ptr) 
{
  // register hash manager and load affix data from aff file
  pHMgr = ptr;
  trystring = NULL;
  encoding=NULL;
  reptable = NULL;
  numrep = 0;
  compound=NULL;
  cpdmin = 3;  // default value
  for (int i=0; i < SETSIZE; i++) {
     pStart[i] = NULL;
     sStart[i] = NULL;
  }
  if (parse_file(affpath)) {
     fprintf(stderr,"Failure loading aff file %s\n",affpath);
     fflush(stderr);
  }
}


AffixMgr::~AffixMgr() 
{
 
  // pass through linked prefix entries and clean up
  for (int i=0; i < SETSIZE ;i++) {
       PfxEntry * ptr = (PfxEntry *)pStart[i];
       PfxEntry * nptr = NULL;
       while (ptr) {
            nptr = ptr->getNext();
            delete(ptr);
            ptr = nptr;
            nptr = NULL;
       }  
  }

  // pass through linked suffix entries and clean up
  for (int j=0; j < SETSIZE ; j++) {
       SfxEntry * ptr = (SfxEntry *)sStart[j];
       SfxEntry * nptr = NULL;
       while (ptr) {
            nptr = ptr->getNext();
            delete(ptr);
            ptr = nptr;
            nptr = NULL;
       }  
  }

  if (trystring) free(trystring);
  trystring=NULL;
  if (encoding) free(encoding);
  encoding=NULL;
  if (reptable) {  
     for (int j=0; j < numrep; j++) {
        free(reptable[j].pattern);
        free(reptable[j].replacement);
        reptable[j].pattern = NULL;
        reptable[j].replacement = NULL;
     }
     free(reptable);  
     reptable = NULL;
  }
  numrep = 0;
  if (compound) free(compound);
  compound=NULL;
  pHMgr = NULL;
  cpdmin = 0;
}


// read in aff file and build up prefix and suffix entry objects 
int  AffixMgr::parse_file(const char * affpath)
{

  // io buffers
  char line[MAXLNLEN+1];
 
  // affix type
  char ft;

  // open the affix file
  FILE * afflst;
  afflst = fopen(affpath,"r");
  if (!afflst) {
    fprintf(stderr,"Error - could not open affix description file %s\n",affpath);
    return 1;
  }

  // step one is to parse the affix file building up the internal
  // affix data structures


    // read in each line ignoring any that do not
    // start with a known line type indicator

    while (fgets(line,MAXLNLEN,afflst)) {
       mychomp(line);

       /* parse in the try string */
       if (strncmp(line,"TRY",3) == 0) {
          if (parse_try(line)) {
             return 1;
          }
       }

       /* parse in the name of the character set used by the .dict and .aff */
       if (strncmp(line,"SET",3) == 0) {
          if (parse_set(line)) {
             return 1;
          }
       }

       /* parse in the flag used by the controlled compound words */
       if (strncmp(line,"COMPOUNDFLAG",12) == 0) {
          if (parse_cpdflag(line)) {
             return 1;
          }
       }

       /* parse in the flag used by the controlled compound words */
       if (strncmp(line,"COMPOUNDMIN",11) == 0) {
          if (parse_cpdmin(line)) {
             return 1;
          }
       }

       /* parse in the typical fault correcting table */
       if (strncmp(line,"REP",3) == 0) {
          if (parse_reptable(line, afflst)) {
             return 1;
          }
       }

       // parse this affix: P - prefix, S - suffix
       ft = ' ';
       if (strncmp(line,"PFX",3) == 0) ft = 'P';
       if (strncmp(line,"SFX",3) == 0) ft = 'S';
       if (ft != ' ') {
          if (parse_affix(line, ft, afflst)) {
             return 1;
          }
       }

    }
    fclose(afflst);

    // now we can speed up performance greatly taking advantage of the 
    // relationship between the affixes and the idea of "subsets".

    // View each prefix as a potential leading subset of another and view
    // each suffix (reversed) as a potential trailing subset of another.

    // To illustrate this relationship if we know the prefix "ab" is found in the
    // word to examine, only prefixes that "ab" is a leading subset of need be examined.
    // Furthermore is "ab" is not present then none of the prefixes that "ab" is
    // is a subset need be examined.
    // The same argument goes for suffix string that are reversed.

    // Then to top this off why not examine the first char of the word to quickly
    // limit the set of prefixes to examine (i.e. the prefixes to examine must 
    // be leading supersets of the first character of the word (if they exist)
 
    // To take advantage of this "subset" relationship, we need to add two links
    // from entry.  One to take next if the current prefix is found (call it nexteq)
    // and one to take next if the current prefix is not found (call it nextne).

    // Since we have built ordered lists, all that remains is to properly intialize 
    // the nextne and nexteq pointers that relate them

    process_pfx_order();
    process_sfx_order();

    return 0;
}


// build a sort order linked list of prefix entries
int AffixMgr::build_pfxlist(AffEntry* pfxptr)
{
  PfxEntry * ptr;
  PfxEntry * pptr;
  PfxEntry * ep = (PfxEntry*) pfxptr;

  /* get the right starting point */
  const char * key = ep->getKey();

  // handle the special case of null affix string
  if (strlen(key) == 0) {
    // always inset them at head of list at element 0
     ptr = (PfxEntry*)pStart[0];
     ep->setNext(ptr);
     pStart[0] = (AffEntry*)ep;
     return 0;
  }

  // now handle the general case
  unsigned char sp = *((const unsigned char *)key);
  ptr = (PfxEntry*)pStart[sp];
  
  /* handle the insert at top of list case */
  if ((!ptr) || ( strcmp( ep->getKey() , ptr->getKey() ) <= 0)) {
     ep->setNext(ptr);
     pStart[sp] = (AffEntry*)ep;
     return 0;
  }

  /* otherwise find where it fits in order and insert it */
  pptr = NULL;
  for (; ptr != NULL; ptr = ptr->getNext()) {
    if (strcmp( ep->getKey() , ptr->getKey() ) <= 0) break;
    pptr = ptr;
  }
  pptr->setNext(ep);
  ep->setNext(ptr);
  return 0;
}



// build a sort order linked list of suffix entries (affix reveresed)
int AffixMgr::build_sfxlist(AffEntry* sfxptr)
{
  SfxEntry * ptr;
  SfxEntry * pptr;
  SfxEntry * ep = (SfxEntry *) sfxptr;

  /* get the right starting point */
  const char * key = ep->getKey();

  // handle the special case of null affix string
  if (strlen(key) == 0) {
    // always inset them at head of list at element 0
     ptr = (SfxEntry*)sStart[0];
     ep->setNext(ptr);
     sStart[0] = (AffEntry*)ep;
     return 0;
  }

  // now handle the normal case
  unsigned char sp = *((const unsigned char *)key);
  ptr = (SfxEntry*)sStart[sp];
  
  /* handle the insert at top of list case */
  if ((!ptr) || ( strcmp( ep->getKey() , ptr->getKey() ) <= 0)) {
     ep->setNext(ptr);
     sStart[sp] = (AffEntry*)ep;
     return 0;
  }

  /* otherwise find where it fits in order and insert it */
  pptr = NULL;
  for (; ptr != NULL; ptr = ptr->getNext()) {
    if (strcmp( ep->getKey(), ptr->getKey() ) <= 0) break;
    pptr = ptr;
  }
  pptr->setNext(ep);
  ep->setNext(ptr);
  return 0;
}



// initialize the PfxEntry links NextEQ and NextNE to speed searching
int AffixMgr::process_pfx_order()
{
    PfxEntry* ptr;

    // loop through each prefix list starting point
    for (int i=1; i < SETSIZE; i++) {

         ptr = (PfxEntry*)pStart[i];

         // look through the remainder of the list
         //  and find next entry with affix that 
         // the current one is not a subset of
         // mark that as destination for NextNE
         // use next in list that you are a subset
         // of as NextEQ

         for (; ptr != NULL; ptr = ptr->getNext()) {

	     PfxEntry * nptr = ptr->getNext();
             for (; nptr != NULL; nptr = nptr->getNext()) {
	         if (! isSubset( ptr->getKey() , nptr->getKey() )) break;
             }
             ptr->setNextNE(nptr);
             ptr->setNextEQ(NULL);
             if ((ptr->getNext()) && isSubset(ptr->getKey() , (ptr->getNext())->getKey())) 
                 ptr->setNextEQ(ptr->getNext());
         }

         // now clean up by adding smart search termination strings:
         // if you are already a superset of the previous prefix
         // but not a subset of the next, search can end here
         // so set NextNE properly

         ptr = (PfxEntry *) pStart[i];
         for (; ptr != NULL; ptr = ptr->getNext()) {
	     PfxEntry * nptr = ptr->getNext();
             PfxEntry * mptr = NULL;
             for (; nptr != NULL; nptr = nptr->getNext()) {
	         if (! isSubset(ptr->getKey(),nptr->getKey())) break;
                 mptr = nptr;
             }
             if (mptr) mptr->setNextNE(NULL);
         }
    }
    return 0;
}



// initialize the SfxEntry links NextEQ and NextNE to speed searching
int AffixMgr::process_sfx_order()
{
    SfxEntry* ptr;

    // loop through each prefix list starting point
    for (int i=1; i < SETSIZE; i++) {

         ptr = (SfxEntry *) sStart[i];

         // look through the remainder of the list
         //  and find next entry with affix that 
         // the current one is not a subset of
         // mark that as destination for NextNE
         // use next in list that you are a subset
         // of as NextEQ

         for (; ptr != NULL; ptr = ptr->getNext()) {
	     SfxEntry * nptr = ptr->getNext();
             for (; nptr != NULL; nptr = nptr->getNext()) {
	         if (! isSubset(ptr->getKey(),nptr->getKey())) break;
             }
             ptr->setNextNE(nptr);
             ptr->setNextEQ(NULL);
             if ((ptr->getNext()) && isSubset(ptr->getKey(),(ptr->getNext())->getKey())) 
                 ptr->setNextEQ(ptr->getNext());
         }


         // now clean up by adding smart search termination strings:
         // if you are already a superset of the previous suffix
         // but not a subset of the next, search can end here
         // so set NextNE properly

         ptr = (SfxEntry *) sStart[i];
         for (; ptr != NULL; ptr = ptr->getNext()) {
	     SfxEntry * nptr = ptr->getNext();
             SfxEntry * mptr = NULL;
             for (; nptr != NULL; nptr = nptr->getNext()) {
	         if (! isSubset(ptr->getKey(),nptr->getKey())) break;
                 mptr = nptr;
             }
             if (mptr) mptr->setNextNE(NULL);
         }
    }
    return 0;
}



// takes aff file condition string and creates the
// conds array - please see the appendix at the end of the
// file affentry.cxx which describes what is going on here
// in much more detail

void AffixMgr::encodeit(struct affentry * ptr, char * cs)
{
  unsigned char c;
  int i, j, k;
  unsigned char mbr[MAXLNLEN];

  // now clear the conditions array */
  for (i=0;i<SETSIZE;i++) ptr->conds[i] = (unsigned char) 0;

  // now parse the string to create the conds array */
  int nc = strlen(cs);
  int neg = 0;   // complement indicator
  int grp = 0;   // group indicator
  int n = 0;     // number of conditions
  int ec = 0;    // end condition indicator
  int nm = 0;    // number of member in group

  // if no condition just return
  if (strcmp(cs,".")==0) {
    ptr->numconds = 0;
    return;
  }

  i = 0;
  while (i < nc) {
    c = *((unsigned char *)(cs + i));

    // start group indicator
    if (c == '[') {
       grp = 1;
       c = 0;
    }

    // complement flag
    if ((grp == 1) && (c == '^')) {
       neg = 1;
       c = 0;
    }

    // end goup indicator
    if (c == ']') {
       ec = 1;
       c = 0;
    }

    // add character of group to list
    if ((grp == 1) && (c != 0)) {
      *(mbr + nm) = c;
      nm++;
      c = 0;
    }

    // end of condition 
    if (c != 0) {
       ec = 1;
    }

    
    if (ec) {
      if (grp == 1) {
        if (neg == 0) {
          // set the proper bits in the condition array vals for those chars
	  for (j=0;j<nm;j++) {
	     k = (unsigned int) mbr[j];
             ptr->conds[k] = ptr->conds[k] | (1 << n);
          }
	} else {
	  // complement so set all of them and then unset indicated ones
	   for (j=0;j<SETSIZE;j++) ptr->conds[j] = ptr->conds[j] | (1 << n);
	   for (j=0;j<nm;j++) {
	     k = (unsigned int) mbr[j];
             ptr->conds[k] = ptr->conds[k] & ~(1 << n);
	   }
        }
        neg = 0;
        grp = 0;   
        nm = 0;
      } else {
         // not a group so just set the proper bit for this char
         // but first handle special case of . inside condition
         if (c == '.') {
	    // wild card character so set them all
            for (j=0;j<SETSIZE;j++) ptr->conds[j] = ptr->conds[j] | (1 << n);
         } else {  
	    ptr->conds[(unsigned int) c] = ptr->conds[(unsigned int)c] | (1 << n);
         }
      }
      n++;
      ec = 0;
    }


    i++;
  }
  ptr->numconds = n;
  return;
}


// check word for prefixes
BasicWordInfo AffixMgr::prefix_check (LookupInfo linf, const char * word, int len)
{
    BasicWordInfo wordinfo;
 
    // first handle the special case of 0 length prefixes
    PfxEntry * pe = (PfxEntry *) pStart[0];
    while (pe) {
       wordinfo = pe->check(linf,word,len);
       if (wordinfo) return wordinfo;
       pe = pe->getNext();
    }
  
    // now handle the general case
    unsigned char sp = *((const unsigned char *)word);
    PfxEntry * pptr = (PfxEntry *)pStart[sp];

    while (pptr) {
        if (isSubset(pptr->getKey(),word)) {
	     wordinfo = pptr->check(linf, word,len);
             if (wordinfo) return wordinfo;
             pptr = pptr->getNextEQ();
        } else {
	     pptr = pptr->getNextNE();
        }
    }
    
    return BasicWordInfo();
}

// // check if compound word is correctly spelled
// BasicWordInfo AffixMgr::compound_check (const char * word, int len, char compound_flag)
// {
//     int i;
//     BasicWordInfo wordinfo= NULL;
//     char * st;
//     char ch;
    
//     // handle case of string too short to be a piece of a compound word 
//     if (len < cpdmin) return NULL;

//     st = mystrdup(word);
    
//     for (i=cpdmin; i < (len - (cpdmin-1)); i++) {

//         ch = st[i];
// 	st[i] = '\0';

// 	wordinfo = lookup(st);
//         if (!wordinfo) wordinfo = affix_check(st,i);

// 	if ((wordinfo) && (TESTAFF(wordinfo->astr, compound_flag, wordinfo->alen))) {
// 	    wordinfo = lookup((word+i));
// 	    if ((wordinfo) && (TESTAFF(wordinfo->astr, compound_flag, wordinfo->alen))) {
// 		free(st);
// 		return wordinfo;
// 	    }
// 	    wordinfo = affix_check((word+i),strlen(word+i));
// 	    if ((wordinfo) && (TESTAFF(wordinfo->astr, compound_flag, wordinfo->alen))) {
// 		free(st);
// 		return wordinfo;
// 	    }
// 	    wordinfo = compound_check((word+i),strlen(word+i),compound_flag); 
// 	    if (wordinfo) {
// 		free(st);
// 		return wordinfo;
// 	    }
	    
// 	}
//         st[i] = ch;
//     }
//     free(st);
//     return NULL;
// }    



// check word for suffixes
BasicWordInfo AffixMgr::suffix_check (LookupInfo linf,
				      const char * word, int len, 
				      int sfxopts, AffEntry * ppfx)
{
    BasicWordInfo wordinfo;

    // first handle the special case of 0 length suffixes
    SfxEntry * se = (SfxEntry *) sStart[0];
    while (se) {
       wordinfo = se->check(linf, word,len, sfxopts, ppfx);
       if (wordinfo) return wordinfo;
       se = se->getNext();
    }
  
    // now handle the general case
    char * tmpword = myrevstrdup(word);
    unsigned char sp = *((const unsigned char *)tmpword);
    SfxEntry * sptr = (SfxEntry *) sStart[sp];

    while (sptr) {
        if (isSubset(sptr->getKey(),tmpword)) {
	     wordinfo = sptr->check(linf, word,len, sfxopts, ppfx);
             if (wordinfo) {
                  free(tmpword);
                  return wordinfo;
             }
             sptr = sptr->getNextEQ();
        } else {
	     sptr = sptr->getNextNE();
        }
    }
    
    free(tmpword);
    return NULL;
}



// check if word with affixes is correctly spelled
BasicWordInfo AffixMgr::affix_check (LookupInfo linf, const char * word, int len)
{
    BasicWordInfo wordinfo;

    // check all prefixes (also crossed with suffixes if allowed) 
    wordinfo = prefix_check(linf, word, len);
    if (wordinfo) return wordinfo;

    // if still not found check all suffixes
    wordinfo = suffix_check(linf, word, len, 0, NULL);
    return wordinfo;
}

// return length of replacing table
int AffixMgr::get_numrep()
{
  return numrep;
}

// return replacing table
struct replentry * AffixMgr::get_reptable()
{
  if (! reptable ) return NULL;
  return reptable;
}

// return text encoding of dictionary
char * AffixMgr::get_encoding()
{
  if (! encoding ) {
      encoding = mystrdup("ISO8859-1");
  }
  return mystrdup(encoding);
}


// return the preferred try string for suggestions
char * AffixMgr::get_try_string()
{
  if (! trystring ) return NULL;
  return mystrdup(trystring);
}

// return the compound words control flag
char * AffixMgr::get_compound()
{
  if (! compound ) return NULL;
  return mystrdup(compound);
}

// strip strings into token based on single char delimiter
// acts like strsep() but only uses a delim char and not 
// a delim string

char * mystrsep(char ** stringp, const char delim)
{
  char * rv = NULL;
  char * mp = *stringp;
  int n = strlen(mp);
  if (n > 0) {
     char * dp = (char *)memchr(mp,(int)((unsigned char)delim),n);
     if (dp) {
        *stringp = dp+1;
        int nc = (int)((unsigned long)dp - (unsigned long)mp); 
        rv = (char *) malloc(nc+1);
        memcpy(rv,mp,nc);
        *(rv+nc) = '\0';
        return rv;
     } else {
       rv = (char *) malloc(n+1);
       memcpy(rv, mp, n);
       *(rv+n) = '\0';
       *stringp = mp + n;
       return rv;
     }
  }
  return NULL;
}


char * mystrdup(const char * s)
{
  char * d = NULL;
  if (s) {
     int sl = strlen(s);
     d = (char *) malloc(((sl+1) * sizeof(char)));
     if (d) memcpy(d,s,((sl+1)*sizeof(char)));
  }
  return d;
}


void mychomp(char * s)
{
  int k = strlen(s);
  if ((k > 0) && ((*(s+k-1) == '\n') || (*(s+k-1) == '\r'))) *(s+k-1) = '\0';
  if ((k > 1) && (*(s+k-2) == '\r')) *(s+k-2) = '\0';
}


char * myrevstrdup(const char * s)
{
    char * d = NULL;
    if (s) {
       int sl = strlen(s);
       d = (char *) malloc((sl+1) * sizeof(char));
       if (d) {
	 const char * p = s + sl - 1;
         char * q = d;
         while (p >= s) *q++ = *p--;
         *q = '\0';
       }
    }
    return d; 
}


/* return 1 if s1 is subset of s2 */
int isSubset(const char * s1, const char * s2)
{
  int l1 = strlen(s1);
  int l2 = strlen(s2);
  if (l1 > l2) return 0;
  if (strncmp(s2,s1,l1) == 0) return 1;
  return 0;
}


/* parse in the try string */
int  AffixMgr::parse_try(char * line)
{
   if (trystring) {
      fprintf(stderr,"error: duplicate TRY strings\n");
      return 1;
   }
   char * tp = line;
   char * piece;
   int i = 0;
   int np = 0;
   while ((piece=mystrsep(&tp,' '))) {
      if (*piece != '\0') {
          switch(i) {
	      case 0: { np++; break; }
              case 1: { trystring = mystrdup(piece); np++; break; }
	      default: break;
          }
          i++;
      }
      free(piece);
   }
   if (np != 2) {
      fprintf(stderr,"error: missing TRY information\n");
      return 1;
   } 
   return 0;
}


/* parse in the name of the character set used by the .dict and .aff */
int  AffixMgr::parse_set(char * line)
{
   if (encoding) {
      fprintf(stderr,"error: duplicate SET strings\n");
      return 1;
   }
   char * tp = line;
   char * piece;
   int i = 0;
   int np = 0;
   while ((piece=mystrsep(&tp,' '))) {
      if (*piece != '\0') {
          switch(i) {
	     case 0: { np++; break; }
             case 1: { encoding = mystrdup(piece); np++; break; }
	     default: break;
          }
          i++;
      }
      free(piece);
   }
   if (np != 2) {
      fprintf(stderr,"error: missing SET information\n");
      return 1;
   } 
   return 0;
}


/* parse in the flag used by the controlled compound words */
int  AffixMgr::parse_cpdflag(char * line)
{
   if (compound) {
      fprintf(stderr,"error: duplicate compound flags used\n");
      return 1;
   }
   char * tp = line;
   char * piece;
   int i = 0;
   int np = 0;
   while ((piece=mystrsep(&tp,' '))) {
      if (*piece != '\0') {
          switch(i) {
	     case 0: { np++; break; }
             case 1: { compound = mystrdup(piece); np++; break; }
	     default: break;
          }
          i++;
      }
      free(piece);
   }
   if (np != 2) {
      fprintf(stderr,"error: missing compound flag information\n");
      return 1;
   }
   return 0;
}


/* parse in the min compound word length */
int  AffixMgr::parse_cpdmin(char * line)
{
   char * tp = line;
   char * piece;
   int i = 0;
   int np = 0;
   while ((piece=mystrsep(&tp,' '))) {
      if (*piece != '\0') {
          switch(i) {
	     case 0: { np++; break; }
             case 1: { cpdmin = atoi(piece); np++; break; }
	     default: break;
          }
          i++;
      }
      free(piece);
   }
   if (np != 2) {
      fprintf(stderr,"error: missing compound min information\n");
      return 1;
   } 
   if ((cpdmin < 1) || (cpdmin > 50)) cpdmin = 3;
   return 0;
}


/* parse in the typical fault correcting table */
int  AffixMgr::parse_reptable(char * line, FILE * af)
{
   if (numrep != 0) {
      fprintf(stderr,"error: duplicate REP tables used\n");
      return 1;
   }
   char * tp = line;
   char * piece;
   int i = 0;
   int np = 0;
   while ((piece=mystrsep(&tp,' '))) {
       if (*piece != '\0') {
          switch(i) {
	     case 0: { np++; break; }
             case 1: { 
                       numrep = atoi(piece);
	               if (numrep < 1) {
			  fprintf(stderr,"incorrect number of entries in replacement table\n");
			  free(piece);
                          return 1;
                       }
                       reptable = (replentry *) malloc(numrep * sizeof(struct replentry));
                       np++;
                       break;
	             }
	     default: break;
          }
          i++;
       }
       free(piece);
   }
   if (np != 2) {
      fprintf(stderr,"error: missing replacement table information\n");
      return 1;
   } 
 
   /* now parse the numrep lines to read in the remainder of the table */
   char * nl = line;
   for (int j=0; j < numrep; j++) {
        fgets(nl,MAXLNLEN,af);
        mychomp(nl);
        tp = nl;
        i = 0;
        reptable[j].pattern = NULL;
        reptable[j].replacement = NULL;
        while ((piece=mystrsep(&tp,' '))) {
           if (*piece != '\0') {
               switch(i) {
                  case 0: {
		             if (strncmp(piece,"REP",3) != 0) {
		                 fprintf(stderr,"error: replacement table is corrupt\n");
                                 free(piece);
                                 return 1;
                             }
                             break;
		          }
                  case 1: { reptable[j].pattern = mystrdup(piece); break; }
                  case 2: { reptable[j].replacement = mystrdup(piece); break; }
		  default: break;
               }
               i++;
           }
           free(piece);
        }
	if ((!(reptable[j].pattern)) || (!(reptable[j].replacement))) {
	     fprintf(stderr,"error: replacement table is corrupt\n");
             return 1;
        }
   }
   return 0;
}


int  AffixMgr::parse_affix(char * line, const char at, FILE * af)
{
   int numents = 0;      // number of affentry structures to parse
   char achar='\0';      // affix char identifier
   short ff=0;
   struct affentry * ptr= NULL;
   struct affentry * nptr= NULL;

   char * tp = line;
   char * nl = line;
   char * piece;
   int i = 0;

   // split affix header line into pieces

   int np = 0;
   while ((piece=mystrsep(&tp,' '))) {
      if (*piece != '\0') {
          switch(i) {
             // piece 1 - is type of affix
             case 0: { np++; break; }
          
             // piece 2 - is affix char
             case 1: { np++; achar = *piece; break; }

             // piece 3 - is cross product indicator 
             case 2: { np++; if (*piece == 'Y') ff = XPRODUCT; break; }

             // piece 4 - is number of affentries
             case 3: { 
                       np++;
                       numents = atoi(piece); 
                       ptr = (struct affentry *) malloc(numents * sizeof(struct affentry));
                       ptr->xpflg = ff;
                       ptr->achar = achar;
                       break;
                     }

	     default: break;
          }
          i++;
      }
      free(piece);
   }
   // check to make sure we parsed enough pieces
   if (np != 4) {
       fprintf(stderr, "error: affix %c header has insufficient data in line %s\n",achar,nl);
       free(ptr);
       return 1;
   }
 
   // store away ptr to first affentry
   nptr = ptr;

   // now parse numents affentries for this affix
   for (int j=0; j < numents; j++) {
      fgets(nl,MAXLNLEN,af);
      mychomp(nl);
      tp = nl;
      i = 0;
      np = 0;

      // split line into pieces
      while ((piece=mystrsep(&tp,' '))) {
         if (*piece != '\0') {
             switch(i) {

                // piece 1 - is type
                case 0: { 
                          np++;
                          if (nptr != ptr) nptr->xpflg = ptr->xpflg;
                          break;
                        }

                // piece 2 - is affix char
                case 1: { 
		          np++;
                          if (*piece != achar) {
                              fprintf(stderr, "error: affix %c is corrupt near line %s\n",achar,nl);
                              fprintf(stderr, "error: possible incorrect count\n");
                              free(piece);
                              return 1;
                          }
                          if (nptr != ptr) nptr->achar = ptr->achar;
                          break;
		        }

                // piece 3 - is string to strip or 0 for null 
                case 2: { 
                          np++;
                          nptr->strip = mystrdup(piece);
                          nptr->stripl = strlen(nptr->strip);
                          if (strcmp(nptr->strip,"0") == 0) {
                              free(nptr->strip);
                              nptr->strip=mystrdup("");
			      nptr->stripl = 0;
                          }   
                          break; 
                        }

                // piece 4 - is affix string or 0 for null
                case 3: { 
		          np++;
                          nptr->appnd = mystrdup(piece);
                          nptr->appndl = strlen(nptr->appnd);
                          if (strcmp(nptr->appnd,"0") == 0) {
                              free(nptr->appnd);
                              nptr->appnd=mystrdup("");
			      nptr->appndl = 0;
                          }   
                          break; 
                        }

                // piece 5 - is the conditions descriptions
                case 4: { np++; encodeit(nptr,piece); }

		default: break;
             }
             i++;
         }
         free(piece);
      }
      // check to make sure we parsed enough pieces
      if (np != 5) {
          fprintf(stderr, "error: affix %c is corrupt near line %s\n",achar,nl);
          free(ptr);
          return 1;
      }
      nptr++;
   }
         
   // now create SfxEntry or PfxEntry objects and use links to
   // build an ordered (sorted by affix string) list
   nptr = ptr;
   for (int k = 0; k < numents; k++) {
      if (at == 'P') {
	  PfxEntry * pfxptr = new PfxEntry(this,nptr);
          build_pfxlist((AffEntry *)pfxptr);
      } else {
	  SfxEntry * sfxptr = new SfxEntry(this,nptr);
          build_sfxlist((AffEntry *)sfxptr); 
      }
      nptr++;
   }      
   free(ptr);
   return 0;
}


}

