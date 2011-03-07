/*
Copyright (c) 1998-2010, Enno Rehling <enno@eressea.de>
                         Katja Zedel <katze@felidae.kn-bremen.de
                         Christian Schlittchen <corwin@amber.kn-bremen.de>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
**/

#ifndef H_KRNL_SHIP
#define H_KRNL_SHIP
#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"

#define DAMAGE_SCALE 100 /* multiplier for sh->damage */

/* ship_type::flags */
#define SFL_OPENSEA 0x01
#define SFL_FLY     0x02
#define SFL_NOCOAST 0x04

typedef struct ship_type {
  const char * name[2];

  int range;  /* range in regions */
  int flags;  /* flags */
  int combat; /* modifier for combat */

  double storm; /* multiplier for chance to drift in storm */
  double damage; /* multiplier for damage taken by the ship */

  int cabins;   /* max. cabins (weight) */
  int cargo;    /* max. cargo (weight) */

  int cptskill; /* min. skill of captain */
  int minskill; /* min. skill to sail this (crew) */
  int sumskill; /* min. sum of crew+captain */

  int fishing;    /* weekly income from fishing */

  int at_bonus;   /* Ver�ndert den Angriffsskill (default: 0)*/
  int df_bonus;   /* Ver�ndert den Verteidigungskill (default: 0)*/
  float tac_bonus;

  const struct terrain_type ** coasts; /* coast that this ship can land on */

  struct construction * construction; /* how to build a ship */
} ship_type;

extern struct quicklist *shiptypes;

/* Alte Schiffstypen: */

extern const ship_type * st_find(const char* name);
extern void st_register(const ship_type * type);

#define NOSHIP NULL

#define SF_DRIFTED 1<<0
#define SF_MOVED   1<<1
#define SF_DAMAGED 1<<2 /* for use in combat */
#define SF_SELECT  1<<3 /* previously FL_DH */
#define SF_FISHING 1<<4 /* was on an ocean, can fish */
#define SF_FLYING  1<<5 /* the ship can fly */

#define SFL_SAVEMASK (SF_FLYING)
#define INCOME_FISHING 10

typedef struct ship {
  struct ship *next;
  struct ship *nexthash;
  int no;
  struct region *region;
  char *name;
  char *display;
  struct attrib * attribs;
  int size;
  int damage; /* damage in 100th of a point of size */
  unsigned int flags;
  const struct ship_type * type;
  direction_t coast;
} ship;

extern void damage_ship(ship *sh, double percent);
extern struct unit *captain(ship *sh);
extern struct unit *shipowner(const struct ship * sh);
extern const char * shipname(const struct ship * self);
extern int shipcapacity(const struct ship * sh);
extern void getshipweight(const struct ship * sh, int *weight, int *cabins);

extern ship *new_ship(const struct ship_type * stype, const struct locale * lang, struct region * r);
extern const char *write_shipname(const struct ship * sh, char * buffer, size_t size);
extern struct ship *findship(int n);
extern struct ship *findshipr(const struct region *r, int n);

extern const struct ship_type * findshiptype(const char *s, const struct locale * lang);

extern void write_ship_reference(const struct ship * sh, struct storage * store);

extern void remove_ship(struct ship ** slist, struct ship * s);
extern void free_ship(struct ship * s);
extern void free_ships(void);

extern const char * ship_getname(const struct ship * self);
extern void ship_setname(struct ship * self, const char * name);

#ifdef __cplusplus
}
#endif
#endif
