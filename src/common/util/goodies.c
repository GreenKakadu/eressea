/* vi: set ts=2:
 *
 *	$Id: goodies.c,v 1.6 2001/02/11 20:54:01 enno Exp $
 *	Eressea PB(E)M host Copyright (C) 1998-2000
 *      Christian Schlittchen (corwin@amber.kn-bremen.de)
 *      Katja Zedel (katze@felidae.kn-bremen.de)
 *      Henning Peters (faroul@beyond.kn-bremen.de)
 *      Enno Rehling (enno@eressea-pbem.de)
 *      Ingo Wilken (Ingo.Wilken@informatik.uni-oldenburg.de)
 *
 *  based on:
 *
 * Atlantis v1.0  13 September 1993 Copyright 1993 by Russell Wallace
 * Atlantis v1.7                    Copyright 1996 by Alex Schr�der
 *
 * This program may not be used, modified or distributed without
 * prior permission by the authors of Eressea.
 * This program may not be sold or used commercially without prior written
 * permission from the authors.
 */

#include <config.h>
#include "goodies.h"

/* libc includes */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>


/* Simple Integer-Liste */

int *
intlist_init(void)
{
	return (calloc(1, sizeof(int)));
}

int *
intlist_add(int *i_p, int i)
{
	i_p[0]++;
	i_p = realloc(i_p, (i_p[0]+1) * sizeof(int));

	i_p[i_p[0]] = i;
	return (i_p);
}

int *
intlist_find(int *i_p, int fi)
{
	int i;

	for(i=1; i <= i_p[0]; i++) {
		if(i_p[i] == fi) return (&i_p[i]);
	}
	return NULL;
}

unsigned int
hashstring(const char* s)
{
	unsigned int key = 0;
	int i = strlen(s);

	while (i>0) {
		key = (s[--i] + key*37);
	}
	return key;
}

char *
escape_string(char * str, char replace)
{
	char * c = str;
	while (*c) {if (isspace(*c)) *c = replace; c++;}
	return str;
}

char *
unescape_string(char * str, char replace)
{
	char * c = str;
	while (*c) { if (*c==replace) *c = ' '; c++; }
	return str;
}

char *
set_string (char **s, const char *neu)
{
	*s = realloc(*s, strlen(neu) + 1);
	return strcpy(*s, neu);
}
