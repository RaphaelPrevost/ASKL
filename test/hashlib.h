#ifndef M_HASHLIB_H
#define M_HASHLIB_H

#include "../lib/askl.h"

/** @defgroup hashtable core::hashtable */

/*  Copyright (C) 2002 by C.B. Falconer

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
  USA

  I can be contacted by mailto:cbfalconer@worldnet.att.net
           or at 680 Hartford Tpk., Hamden, CT 06517, USA.

  Should you wish to incorporate this in proprietary software for
  sale, contact me and we can discuss a different license, such
  as the GNU Library license or other mutually agreeable terms.

  =========== significant revisions ============

  v 0.0.0.5 Largely due to prodding from:
      jmccarty@sun1307.ssd.usa.alcatel.com (Mike Mccarty Sr)
  I have come to the conclusion that a hshdelete function is
  implementable.  It has been added. This also requires that
  a 'hdeleted' value be added to the status.
  The table actually holds hentries-hdeleted active entries.
*/

/*
 * This is an example of object oriented programming in C, in
 * that it isolates the hashtable functioning from the objects
 * it stores and retrieves.  It is expected to be useful in
 * such areas as symbol tables and Markov chains.
 *
 * Note that the user need not be concerned with the table size.
 *
 * This library is intended to be totally re-entrant and thread
 * safe.  It can provide storage and lookup for arbitrary data
 * items of arbitrary types.  The hashtable_free() function will
 * release all associated storage, after which hashtable_alloc()
 * is needed before using the database again.
 *
 * The pointers returned by hashtable_insert() and hashtable_find()
 * may be used to modify the data items, PROVIDED THAT such does
 * NOT affect the values returned from cmp() or hash() or
 * rehash() in any way.  Note that these returned pointers
 * are normally NOT the same as the pointers passed in to those
 * two functions.  This is controlled in hshdupe and hshundupe
 *
 */

/* opaque incomplete object */
typedef struct hshtag hashtable;

/* Possible error returns, powers of 2 */
enum hsherr {HASH_SUCCESS = 0, HASH_ENOMEM, HASH_EFULL, HASH_EINTR = 4};

/* NOTE: probes and misses aids evaluating hash functions */
typedef struct hshstats {
   unsigned long probes, misses,  /* usage statistics */
                 hentries,        /* current entries count */
                 hdeleted;        /* deleted entry count */
   enum hsherr   herror;          /* error status */
   unsigned int  version;         /* hashlib version */
} m_hashstats;

/* Note: version when expressed in decimal, is of the form": */
/* M.n.v.p  where  M = Major version                         */
/*                 n = minor version                         */
/*                 v = variation                             */
/*                 p = patch                                 */
/* with all values except M being single decimal digits.     */


/* -------------- The auxiliary function types ---------------- */
/* The actual storage for the various void * item pointers is   */
/* known to the calling program, but not to this library. These */
/* function pointers allow the library to adapt itself to       */
/* arbitrary types of data.                                     */
/*                                                              */
/* When called from the hashlib system the 'item' parameter to  */
/* these functions will never be NULL.  The application is thus */
/* free to use such a value for any special purpose, such as    */
/* re-initialization of static variables.                       */

/* a hashfn() returns some hashed value from the item           */
/* Note that two functions of this form must be provided        */
/* The quality of these functions strongly affects performance  */
typedef unsigned long (*fp_hash)(const void *item);

/* A hshcmpfn() compares two items, and returns -ve, 0 (equal), +ve */
/* corresponding to litem < ritem, litem == ritem, litem > ritem    */
/* It need only return zero/non-zero if not to be used elsewhere    */
typedef int  (*fp_hashcmp)(const void *litem, const void *ritem);

/* A hshdupfn() duplicates the item into malloced space.  Further   */
/* management of the duplicated space is the libraries duty.  It is */
/* only used via hshinsert(), and must return NULL for failure.     */
/* The application is free to modify fields of the allocated space, */
/* provided such modification does NOT affect hshcmpfn              */
typedef void *(*fp_hashdup)(const void *item);

/* A hshfreefn() reverses the action of a hshdupfn.  It is only     */
/* called during execution of the hshkill() function.  This allows  */
/* clean-up of memory malloced within the hshdupfn. After execution */
/* of hshfree the item pointer should normally not be used.  See    */
/* sort demo for an exception.                                      */
typedef void (*fp_hashfree)(void *item);

/* A hshexecfn() performs some operation on a data item.  It may be */
/* passed additional data in datum.  It is only used in walking the */
/* complete stored database. It returns 0 for success.              */
/* xtra will normally be NULL, but may be used for debug purposes   */
/* During a database walk, the item parameter will never be NULL    */
typedef int (*fp_hashexec)(void *item, void *datum, void *xtra);

/* ------------ END of auxiliary function types ------------- */

/* -------------------------------------------------------------------------- */

/* initialize and return a pointer to the data base */
public hashtable *hashlib_alloc(const fp_hash hash, const fp_hash rehash,
                                const fp_hashcmp cmp, const fp_hashdup dupe,
                                const fp_hashfree undupe, int hdebug);

/* -------------------------------------------------------------------------- */

/* destroy the data base. Accepts NULL and does nothing */
public void hashlib_free(hashtable *master);

/* -------------------------------------------------------------------------- */

public void *hashlib_insert(hashtable *master, const void *item);

/**
 * @ingroup hashtable
 * @fn void *hashtable_insert(mhashlib *master, void *item)
 * @param master a pointer to a hashtable
 * @param item a pointer to the item to insert
 * @return the item, or NULL.
 *
 * This function inserts a new entry in the given hashtable, and returns it
 * on success. If an error occurs, this function will return NULL.
 *
 */

/* -------------------------------------------------------------------------- */

public void *hashlib_find(hashtable *master, const void *item);

/**
 * @ingroup hashtable
 * @fn void *hashtable_find(mhashlib *master, void *item)
 * @param master a pointer to a hashtable
 * @param item the item to locate
 * @return a pointer to the item if it is found, NULL otherwise.
 *
 * This function search through the hashtable for the given item, and returns
 * a pointer to it if it is found, NULL otherwise.
 *
 */

/* -------------------------------------------------------------------------- */

public void *hashlib_remove(hashtable *master, const void *item);

/**
 * @ingroup hashtable
 * @fn void *hashtable_remove(mhashlib *master, void *item)
 * @param master a pointer to a hashtable
 * @param item a pointer to an item
 * @return the given item, or NULL if it was not found.
 *
 * This function deletes an existing entry from the hashtable, and returns it.
 * Disposal of the storage returned by hshdelete (originally created by hshdupfn)
 * is up to the application. It is no longer managed by hashlib.
 * It will usually be disposable by hshfreefn().
 *
 */

/* -------------------------------------------------------------------------- */

public int hashlib_walk(hashtable *master, const fp_hashexec exec, void *datum);

/**
 * @ingroup hashtable
 * @fn int hashtable_walk(mhashlib *master, fp_hashexec exec, void *datum)
 * @param master a pointer to a hashtable
 * @param exec a function pointer
 * @param datum memory passed to exec
 * @return 0 on success, a non-zero error code otherwise.
 *
 * This function calls the function pointer @b exec on each entry in the
 * table. If the @b exec callback returns a non-zero value, the walk is
 * stopped and hashtable_walk fails.
 *
 * The @b datum parameter can be used to provide some working memory space
 * to the @b exec callback.
 *
 */

/* -------------------------------------------------------------------------- */

/* return various statistics on use of this hshtbl */
public m_hashstats hashlib_stats(const hashtable *master);

/* return the overhead consumed by the hashtable */
public size_t hashlib_footprint(const hashtable *master);

/* -------------------------------------------------------------------------- */
/* Useful generic functions */
/* -------------------------------------------------------------------------- */

/* Hash a string quantity */
public unsigned long hashlib_strhash(const char *string);

/* -------------------------------------------------------------------------- */

/* ReHash a string quantity */
public unsigned long hashlib_strrehash(const char *string);

#endif
