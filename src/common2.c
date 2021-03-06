/*
 * common2.c
 */

/* $Id: common2.c,v 1.6 1993/09/03 01:04:12 beust Exp $ */

#include <libraries/asl.h>

#include "common.h"

/***********
 * Private
 ***********/

static void
removeFakes(Entries list)
/* Remove all fake entries */
{
   DataBase db = (DataBase) list -> list;
   Entry entry;

   DB_Rewind(db);
   while ((entry = (Entry) DB_NextEntry(db))) {
      if (entry -> fake) {
         DB_RemoveEntry(db, entry);
      }
   }
}

static void
calculateNewFakes(Entries list)
{
   struct DateStamp current;
   struct DateTime dt;
   Automatic aut;
   struct _Entry entry;
   ULONG day, min, tick;
   ULONG date, month, year;
   ULONG s, ps, stop;
   char *resultDate, bool;
   DataBase db = (DataBase) list -> automatic;

   DateStamp(& current);
   resultDate = (char *) malloc(64);

   DB_Rewind(db);
   while ((aut = (Automatic) DB_NextEntry(db))) {
      s = aut -> first.ds_Days;

/* Build the fake entry */
      strcpy(entry.amount, aut -> amount);
      strcpy(entry.transaction, aut -> transaction);
      strcpy(entry.imputation, aut -> imputation);
      entry.validated = 1;

/* If no end has been given, use the current time as a stop value */

      stop = (aut -> last.ds_Days ? aut -> last.ds_Days : current.ds_Days);

      while (s < stop) {

       /* Get month and year corresponding to this date */
         dt.dat_Stamp.ds_Days = s;
         dt.dat_Stamp.ds_Minute = 0;
         dt.dat_Stamp.ds_Tick = 0;

       /* Copy it to the fake entry, and add it to the global list */
         memcpy(& entry.date, & dt.dat_Stamp, sizeof(dt.dat_Stamp));
         addEntryDatabase(list, & entry, 1, aut);
                 /* 1 because it's a fake entry, generated by aut */

       /* Now compute the next automatic occurence */
         dt.dat_StrDate = resultDate;
         dt.dat_StrDay = NULL;
         dt.dat_StrTime = NULL;
         dt.dat_Format = FORMAT_CDN;
         dt.dat_Flags = NULL; /* could be DTF_SUBST to have Monday when possible */
         bool = DateToStr(& dt);
         sscanf(resultDate,"%d-%d-%d", & date, & month, & year);

       /* Now we have the human-readable form, perform an intelligent increment */
         ps = s;
         s += aut -> repeatDays;
         s += aut -> repeatWeeks * 7;
         s += daysInNMonths(ps, aut -> repeatMonths, month, year);
         s += daysInNYears(ps, aut -> repeatYears, year);
      }
   }
}


/***********
 * Public
 ***********/

TotalType
computeTotal(Entries list)
{
   TotalType result = 0.0, tmp = 0.0;
   char *p;
   DataBase db = (DataBase) list -> list;
   Entry entry;
   int i;

   DB_Rewind(db);
   while (1) {
      entry = (Entry) DB_NextEntry(db);
      if (! entry) break;
      p = entry -> amount;
      result += STRINGTOTOTAL(p);
   }

   return result;
}

TotalType
computeValidatedTotal(Entries list)
{
   TotalType result = 0.0, tmp = 0.0;
   char *p;
   DataBase db = (DataBase) list -> list;
   Entry entry;
   int i;

   DB_Rewind(db);
   while (1) {
      entry = (Entry) DB_NextEntry(db);
      if (! entry) break;
      p = entry -> amount;
      if (entry -> validated) result += STRINGTOTOTAL(p);
   }

   return result;
}


void
addEntryDatabase(Entries entries, Entry entry, char fake, Automatic generatedBy)
{
   entry -> generatedBy = generatedBy;
   entry -> fake = fake;
   DB_AddEntry((DataBase) entries -> list, entry);

}

void
buildDataBase(Entries entries)
{
   removeFakes(entries);
   calculateNewFakes(entries);
}

char *
returnFormattedString(struct StringAspect *sa, Entry entry)
{
   int i;
   char *result, string[128], substring[128];
   char *value;

   string[0] = '\0';
   for (i = 0; i < sa -> numberOfFields; i++) {
      switch(sa -> field[i]) {
         case DATEFIELD :
            value = myDateToStr(& entry -> date);
            break;
         case AMOUNTFIELD :
            value = entry -> amount;
            break;
         case TRANSACTIONFIELD :
            value = entry -> transaction;
            break;
         case IMPUTATIONFIELD :
            value = entry -> imputation;
            break;
         case REASONFIELD :
            value = entry -> reason;
            break;
         case CHECKNUMBERFIELD:
            value = entry -> checkNumber;
            break;
         default :
            myMsg1("returnformattedstring, unexpected case");
      }
      sprintf(substring, sa -> format[i], value);
      strcat(string, substring);
   }

   result = (char *) malloc(strlen(string) + strlen(sa -> after) + 2);

/* Add trailing characters */
   strcpy(result, string);
   strcat(result, sa -> after);

   return result;
}

void
sortList(Entries list)
{
/*
   int count = DB_Count((DataBase) list -> list);
   DB_Sort((DataBase) list -> list, compareTwoEntries);
*/
}


int
daysInNMonths(ULONG days, ULONG n, ULONG month, ULONG year)
{
   int result = 0;
   ULONG months[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
   char monthName[10];


   month--;
   while (n--) {
      result += months[month];
      if (year % 4 == 0 && month == 1) result++;
      month++;
      month %= 12;
      if (month == 0) year++;
   }
   return result;
}

int
daysInNYears(ULONG days, ULONG n, ULONG year)
{
   int result = 0;

   while (n--) {
      result += 365;
      if (year % 4 == 0) result++;
      year++;
   }
   return result;
}

char *
printEntry(Entries list, int number, char *s)
{
   Entry entry = DB_NthEntry((DataBase) list -> list, number);

   sprintf(s, ENTRYFORMATSTRING, (entry -> validated ? ' ' : '*'),
                         entry -> date, entry -> transaction, entry -> amount,
                         entry -> checkNumber, entry -> imputation, entry -> reason);

   return s;
}

void
printEntries(Entries list)
{
   int count, i;
   char s[132];

   count = DB_Count((DataBase) list);
   printf("%d entries\n", count);

   printf("------------------\n");
   for (i=0; i < count; i++)
      printf("%s\n", printEntry(list, i, s));
   printf("------------------\n");
}


void
parseListEntry(char *p, struct ListEntry *le)
{
   int columnNumber = 0, fieldNumber = 0;
   char format[256];    /* will hold the format, one per column */
   char *pb, before[60];
   char *pk, keyword[60];

   strcpy(le -> col[0].fields, "");
   while (1) {
      if (*p && ! isalpha(*p) && *p != '|' && *p != '"') { p++; continue; }
      strcpy(before, "");

      if (*p == '\0' || *p == '|') {
         le -> col[columnNumber].format = strdup(format);
         le -> col[columnNumber].numberOfFields = fieldNumber;
         strcpy(format, "");

         columnNumber++;
    fieldNumber = 0;
         strcpy(le -> col[columnNumber].fields, "");
         if (*p == '|') p++;
         while (*p && isblank(*p)) p++;    /* skip possible blanks before the # */
    if (*p =='\0') break;
      }

      if (*p == '"') {         /* *p is not blank */
         p++;
         pb = before;
         while (*p && *p != '"') *pb++ = *p++;
         *pb++ = '\0';
         if (*p == '"') p++;
         while (*p && isblank(*p)) p++;    /* skip possible blanks before the # */
    strcat(format, before);
         continue;
      }

      pk = keyword;            /* *p is not blank */
      while (*p && isalpha(*p)) *pk++ = *p++;
      *pk++ = '\0';
      if (stricmp(keyword, "date") == 0) {
         le -> col[columnNumber].fields[fieldNumber++] = (char) DATEFIELD;
    strcat(format, "%s");
      }
      else if (stricmp(keyword, "amount") == 0) {
         le -> col[columnNumber].fields[fieldNumber++] = (char) AMOUNTFIELD;
    strcat(format, "%s");
      }
      else if (stricmp(keyword, "transaction") == 0) {
         le -> col[columnNumber].fields[fieldNumber++] = (char) TRANSACTIONFIELD;
    strcat(format, "%s");
      }
      else if (stricmp(keyword, "imputation") == 0) {
         le -> col[columnNumber].fields[fieldNumber++] = (char) IMPUTATIONFIELD;
    strcat(format, "%s");
      }
      else if (stricmp(keyword, "reason") == 0) {
         le -> col[columnNumber].fields[fieldNumber++] = (char) REASONFIELD;
    strcat(format, "%s");
      }
      else if (stricmp(keyword, "checknumber") == 0) {
         le -> col[columnNumber].fields[fieldNumber++] = (char) CHECKNUMBERFIELD;
    strcat(format, "%s");
      }

      else myMsg2("readconfig : unknown keyword", keyword);

      strcat(format, before);
   }

   le -> numberOfColumns = columnNumber;
}

struct StringAspect *
parseFormatString(char *string)
/* [[@n &n] keyword string] * */
{
   struct StringAspect *result;
   char *p, *pk, keyword[32], finalString[64], subString[64],
        field[64], before[64], *pa;
   long mask = 0;
   int col = 0, fieldNumber = 0, size = 0;

   p = string;

   NEW(result, struct StringAspect);

   col = size = 0;
   before[0] = '\0';
   while (1) {
      if (! *p) break;
      if (! isalpha(*p) && *p != '@' && *p != '&' && *p != '"') { p++; continue; }

      if (*p == '@') {         /* *p is not blank */
         p++;
         col = atoi(p); 
         while (*p && isblank(*p)) p++;    /* skip possible blanks before the # */
         while (*p && isdigit(*p)) p++;    /* now the number is read, skip it */
         continue;
      }

      if (*p == '&') {         /* *p is not blank */
         p++;
         size = atoi(p); 
         while (*p && isblank(*p)) p++;    /* skip possible blanks before the # */
         while (*p && isdigit(*p)) p++;    /* now the number is read, skip it */
         continue;
      }

      if (*p == '"') {         /* *p is not blank */
         p++;
         pa = before;
         while (*p && *p != '"') *pa++ = *p++;
         *pa++ = '\0';
         if (*p == '"') p++;
         while (*p && isblank(*p)) p++;    /* skip possible blanks before the # */
         continue;
      }

      pk = keyword;            /* *p is not blank */
      while (*p && isalpha(*p)) *pk++ = *p++;
      *pk++ = '\0';
      if (stricmp(keyword, "date") == 0) {
         result -> field[fieldNumber] = DATEFIELD;
      }
      else if (stricmp(keyword, "amount") == 0) {
         result -> field[fieldNumber] = AMOUNTFIELD;
      }
      else if (stricmp(keyword, "transaction") == 0) {
         result -> field[fieldNumber] = TRANSACTIONFIELD;
      }
      else if (stricmp(keyword, "imputation") == 0) {
         result -> field[fieldNumber] = IMPUTATIONFIELD;
      }
      else if (stricmp(keyword, "reason") == 0) {
         result -> field[fieldNumber]= REASONFIELD;
      }
      else if (stricmp(keyword, "checknumber") == 0) {
         result -> field[fieldNumber]= CHECKNUMBERFIELD;
      }

      else myMsg2("readconfig : unknown keyword", keyword);

   /* Now we have result -> field[fieldNumber]. Take care of result -> format */
      finalString[0] = 0;

   /* Add the possible "before" field */
      strcat(finalString, before);

      if (size)
         sprintf(subString, "%%%ds", size);
      else
         sprintf(subString, "%%s");
      strcat(finalString, subString);

      result -> format[fieldNumber] = (char *) malloc(strlen(finalString) + 2);
      strcpy(result -> format[fieldNumber], finalString);

      if (fieldNumber < MAX_FIELDS - 1) fieldNumber++;
      col = size = 0;
      before[0] = '\0';
   }

/* And finally, store the number of fields the user wants */
   result -> numberOfFields = fieldNumber;

   result -> after = (char *) malloc(strlen(before) + 2);
   strcpy(result -> after, before);

   return result;
}

void
clearDataBase(Entries entries)
{
   DB_ClearDataBase((DataBase) entries -> list);
   DB_ClearDataBase((DataBase) entries -> automatic);
   strcpy(FileName, getString(MSG_NO_FILE_LOADED));
}

void
getFileName(Gui_t gui, char *initial, char *hail, char *result, char *pattern)
{
   struct FileRequester *fr;
   char *path, *file;
   APTR winObject = gui -> mainWindowObject;
   struct Window *win;

   get(winObject, MUIA_Window_Window, & win);
   fr = (struct FileRequester *) AllocAslRequestTags(ASL_FileRequest,
                              ASL_Hail, hail,
                              TAG_DONE);

   if (AslRequestTags(fr,
            ASLFR_Locale, MyLocale,
/*
            ASLFR_Screen, MyScreen,
*/
            ASLFR_SleepWindow, TRUE,
            ASLFR_Window, win,
            ASLFR_InitialDrawer, myPathPart(initial),
            ASLFR_InitialFile, FilePart(initial),
            ASLFR_InitialPattern, pattern,
            TAG_END)) {

      path = fr -> rf_Dir;
      file = fr -> rf_File;

   /* update the filename */
      if (path[0] && path[strlen(path)-1] != ':')
         sprintf(result,"%s/%s", path, file);
      else
         sprintf(result,"%s%s", path, file);

   /* update the pattern if the user changed it */
      strcpy(pattern, fr -> fr_Pattern);
   }
}

int
showRequester(Gui_t gui, char *title, char *text, char *choices)
{
   int result;
   struct EasyStruct es;
   struct Window *win;
   APTR winObject;
   extern struct Window *MyWindow;

   get(gui -> app, MUIA_Application_Window, & winObject);
   if (winObject) {
      result = MUI_Request(gui -> app, NULL, 0, title, choices, text);
/*
     es.es_StructSize = sizeof(es);
     es.es_Flags = 0;
     es.es_Title = title;
     es.es_TextFormat = text;
     es.es_GadgetFormat = choices;

     result = EasyRequest(win, & es, NULL, NULL);
*/
   }
   else {
     result = -1;
   }

   return result;
}

char *
myPathPart(char *path)
{
   char *p = path, *result, isAPath = 0, *endPath;
   while (*p) {
      if (*p == '/' || *p == ':') { isAPath = 1; endPath = p; }
      p++;
   }
   if (! isAPath) return NULL;
   else {
      int len = endPath - path;
      result = (char *) malloc(len + 2);
      memcpy(result, path, len+1);
      result[len+1] = 0;
      return result;
   }
}

char *
mySprintf(char *s, char *f, LONG *arg)
{
   int n = 0;
   LONG *p = arg;
   while (*p != NULL) p++;
   n = p - arg;
   switch (n) {
      case 0 : sprintf(s, f); break;
      case 1 : sprintf(s, f, arg[0]); break;
      case 2 : sprintf(s, f, arg[0], arg[1]); break;
      case 3 : sprintf(s, f, arg[0], arg[1], arg[2]); break;
      case 4 : sprintf(s, f, arg[0], arg[1], arg[2], arg[3]); break;
      case 5 : sprintf(s, f, arg[0], arg[1], arg[2], arg[3], arg[4]); break;
   }
   return f;
}

void
buildList(Gui_t gui, Entries entries, BOOL entry, BOOL sorted,
     struct SmartAlloc *sn)
{
   int countEntry, countAuto, i = 0, count;
   char *date, *name;
   Entry nextEntry;
   Automatic nextAuto;
   Generic gen;
   DataBase db;
   APTR object, newEntry;
   char result[256];

   buildDataBase(entries);
   if (sorted) sortList(entries);

   db = (entry ? (DataBase) entries -> list : (DataBase) entries -> automatic);
   object = (entry ? gui -> liListObject : gui -> auListObject);
   DoMethod(object, MUIM_List_Clear);

   countEntry = DB_Count((DataBase) entries -> list);
   countAuto = DB_Count((DataBase) entries -> automatic);
   if (entry && countEntry == 0) return;
   if (! entry && countAuto == 0) return;


   count = DB_Count(db);
   if (sn -> count < count) {   /* must allocate a new list */
     char **oldStrings = sn -> strings, **newStrings;
     newStrings = (char **) malloc(sizeof(char *) * count);
     for (i = 0; i < count; i++) {
       newStrings[i] = (char *) malloc(256);
/*
   printf("freeing 1\n");
       if (oldNodes[i].ln_Name) free(oldNodes[i].ln_Name);
   printf("freeing 2\n");
       free(& oldNodes[i]);
*/
     }
     sn -> count = count;
     sn -> strings = newStrings;
   }

/* Disable and clear the list object */
   set(object, MUIA_List_Quiet, TRUE);

/* Build up the node list, with a different string if it is an Entry or an Auto */
   i = 0;
   DB_Rewind(db);
   while (! DB_EndOfDataBase(db)) {
      name = sn -> strings[i];
      gen = DB_NextEntry(db);
      if (entry) {
         nextEntry = (Entry) gen;
         sprintf(name, "%c %s",
                  (nextEntry -> fake ? '+' : (nextEntry -> validated ? ' ' : '*')),
                  returnFormattedString(& DisplayFormatStructure, nextEntry));
         nextEntry -> formattedString = name;
         newEntry = (APTR) nextEntry;
      }
      else {
         nextAuto = (Automatic) gen;
         date = myDateToStr(& nextAuto -> first);
         sprintf(name, AUTOFORMATSTRING,
                                           '+',
                                           date,
                                           nextAuto -> repeatDays,
                                           nextAuto -> repeatWeeks,
                                           nextAuto -> repeatMonths,
                                           nextAuto -> repeatYears,
                                           nextAuto -> transaction);
    nextAuto -> formattedString = name;
    newEntry = (APTR) nextAuto;
      
      }
      DoMethod(object, MUIM_List_Insert, & newEntry, 1, MUIV_List_Insert_Sorted);
      i++;
   }

/* Enable the list object */
   set(object, MUIA_List_Quiet, FALSE);
}

