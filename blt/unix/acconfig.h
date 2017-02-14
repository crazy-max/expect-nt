/* acconfig.h
   This file is in the public domain.

   Descriptive text for the C preprocessor macros that
   the distributed Autoconf macros can define.
   No software package will use all of them; autoheader copies the ones
   your configure.in uses into your configuration header file templates.

   The entries are in sort -df order: alphabetical, case insensitive,
   ignoring punctuation (such as underscores).  Although this order
   can split up related entries, it makes it easier to check whether
   a given entry is in the file.

   Leave the following blank line there!!  Autoheader needs it.  */


/* Define if DBL_EPSILON is not defined in float.h */
#undef BLT_DBL_EPSILON

/* Define if strcasecmp isn't declared in a standard header file. */
#undef NO_DECL_STRCASECMP

/* Define if strdup isn't declared in a standard header file. */
#undef NO_DECL_STRDUP

/* Define if compiler can't handle long strings.  */
#undef NO_INLINE_PS_PROLOG

/* Define if union wait type is defined incorrectly.  */
#undef NO_UNION_WAIT

