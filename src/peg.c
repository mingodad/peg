/* Copyright (c) 2007 by Ian Piumarta
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the 'Software'),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, provided that the above copyright notice(s) and this
 * permission notice appear in all copies of the Software.  Acknowledgement
 * of the use of this Software in supporting documentation would be
 * appreciated but is not required.
 * 
 * THE SOFTWARE IS PROVIDED 'AS IS'.  USE ENTIRELY AT YOUR OWN RISK.
 * 
 * Last edited: 2016-07-22 09:45:47 by piumarta on zora.local
 */

#include "tree.h"
#include "version.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <assert.h>

FILE *input= 0;

int   verboseFlag= 0;
int   ebnfFlag= 0;
int   nakedFlag= 0;
int   legFlag= 0;
int   pegjsFlag= 0;
int   nolinesFlag= 0;

static int   lineNumber= 0;
static int   inputPos= 0;
static int   lineNumberPos= 0;
static int   actionLine= 0;
static char *fileName= 0;
static char *outfileName= 0;
static int   headerLine= 0;
static Trailer *trailer= 0;
static Header  *headers= 0;

void yyerror(char *message);

#define YY_INPUT(buf, result, max)			\
{							\
  int c= getc(input);					\
  result= (EOF == c) ? 0 : (*(buf)= c, ++inputPos, 1); \
}

#define YY_LOCAL(T)	static T
#define YY_RULE(T)	static T

#include "peg.peg-c"

void yyerror(char *message)
{
  int line, col;
  yylinecol(yyctx->__buf, yyctx->__begin, &line, &col);
  fprintf(stderr, "%s:%d:%d %s", fileName, line, col, message);
  if (yyctx->__text[0]) fprintf(stderr, " near token '%s'", yyctx->__text);
  if (yyctx->__pos < yyctx->__limit || !feof(input))
    {
      yyctx->__buf[yyctx->__limit]= '\0';
      fprintf(stderr, " %s text \"", yyctx->__pos ? "before" : "at");
      int startpos = yyctx->__pos ? yyctx->__pos : yyctx->__begin;
      while (startpos < yyctx->__limit)
	{
	  if ('\n' == yyctx->__buf[startpos] || '\r' == yyctx->__buf[startpos]) break;
	  fputc(yyctx->__buf[startpos++], stderr);
	}
      if (startpos == yyctx->__limit)
	{
	  int c;
	  while (EOF != (c= fgetc(input)) && '\n' != c && '\r' != c)
	    fputc(c, stderr);
	}
      fputc('\"', stderr);
    }
  fprintf(stderr, "\n");
  exit(1);
}

static void version(char *name)
{
  printf("%s version %d.%d.%d\n", name, PEG_MAJOR, PEG_MINOR, PEG_LEVEL);
}

static void usage(char *name)
{
  version(name);
  fprintf(stderr, "usage: %s [<option>...] [<file>...]\n", name);
  fprintf(stderr, "where <option> can be\n");
  fprintf(stderr, "  -h          print this help information\n");
  fprintf(stderr, "  -o <ofile>  write output to <ofile>\n");
  fprintf(stderr, "  -P          do not generate #line directives\n");
  fprintf(stderr, "  -n          output naked\n");
  fprintf(stderr, "  -l          output leg format\n");
  fprintf(stderr, "  -j          output pegjs/peggy format\n");
  fprintf(stderr, "  -e          output EBNF\n");
  fprintf(stderr, "  -v          be verbose\n");
  fprintf(stderr, "  -V          print version number and exit\n");
  fprintf(stderr, "if no <file> is given, input is read from stdin\n");
  fprintf(stderr, "if no <ofile> is given, output is written to stdout\n");
  exit(1);
}

int main(int argc, char **argv)
{
  Node *n;
  int   c;

  output= stdout;
  input= stdin;
  lineNumber= 1;
  fileName= "<stdin>";

  while (-1 != (c= getopt(argc, argv, "PVho:velnj")))
    {
      switch (c)
	{
	case 'V':
	  version(basename(argv[0]));
	  exit(0);

	case 'h':
	  usage(basename(argv[0]));
	  break;

	case 'o':
          outfileName = optarg;
	  break;

	case 'P':
	  nolinesFlag= 1;
	  break;

	case 'v':
	  verboseFlag= 1;
	  break;

	case 'e':
	  ebnfFlag= 1;
	  break;

	case 'l':
	  legFlag= 1;
	  break;

	case 'n':
	  nakedFlag= 1;
	  break;

	case 'j':
	  pegjsFlag= 1;
	  break;

	default:
	  fprintf(stderr, "for usage try: %s -h\n", argv[0]);
	  exit(1);
	}
    }
  argc -= optind;
  argv += optind;

  if (argc)
    {
      for (;  argc;  --argc, ++argv)
	{
	  if (!strcmp(*argv, "-"))
	    {
	      input= stdin;
	      fileName= "<stdin>";
	    }
	  else
	    {
	      if (!(input= fopen(*argv, "r")))
		{
		  perror(*argv);
		  exit(1);
		}
	      fileName= *argv;
	    }
	  lineNumber= 1;
	  if (!yyparse())
	    yyerror("syntax error");
	  if (input != stdin)
	    fclose(input);
	}
    }
  else
    if (!yyparse())
      yyerror("syntax error");

  if (verboseFlag)
    for (n= rules;  n;  n= n->any.next)
      Rule_print(n);

  if (ebnfFlag)
    EBNF_print();

  if (legFlag)
    LEG_print(nakedFlag);

  if (pegjsFlag)
    PEGJS_print(nakedFlag);

  if(ebnfFlag || legFlag || pegjsFlag)
    return 0;

  if (nakedFlag) {
    PEG_print(nakedFlag);
    return 0;
  }

  if (outfileName && !(output= fopen(outfileName, "w"))) {
    perror(optarg);
    exit(1);
  }

  Rule_compile_c_header();

  for (; headers;) {
    Header *tmp = headers;
    fprintf(output, "#line %i \"%s\"\n%s\n", headers->line, fileName, headers->text);
    headers= headers->next;
    free(tmp);
  }

  if (rules) Rule_compile_c(rules, nolinesFlag);

  if (trailer) {
    if (!nolinesFlag)
      fprintf(output, "#line %i \"%s\"\n", trailer->trailerLine, fileName);
    fprintf(output, "%s\n", trailer->trailer);
    free(trailer);
  }

  return 0;
}
