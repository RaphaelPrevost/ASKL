/*******************************************************************************
 *  ASKL.                                                                      *
 *  Copyright (c) 2025 Raphael Prevost <raph@el.bzh>                           *
 *                                                                             *
 *  This software is a computer program whose purpose is to provide a          *
 *  framework for developing and prototyping network services.                 *
 *                                                                             *
 *  This software is governed by the CeCILL  license under French law and      *
 *  abiding by the rules of distribution of free software.  You can  use,      *
 *  modify and/ or redistribute the software under the terms of the CeCILL     *
 *  license as circulated by CEA, CNRS and INRIA at the following URL          *
 *  "http://www.cecill.info".                                                  *
 *                                                                             *
 *  As a counterpart to the access to the source code and  rights to copy,     *
 *  modify and redistribute granted by the license, users are provided only    *
 *  with a limited warranty  and the software's author,  the holder of the     *
 *  economic rights,  and the successive licensors  have only  limited         *
 *  liability.                                                                 *
 *                                                                             *
 *  In this respect, the user's attention is drawn to the risks associated     *
 *  with loading,  using,  modifying and/or developing or reproducing the      *
 *  software by the user in light of its specific status of free software,     *
 *  that may mean  that it is complicated to manipulate,  and  that  also      *
 *  therefore means  that it is reserved for developers  and  experienced      *
 *  professionals having in-depth computer knowledge. Users are therefore      *
 *  encouraged to load and test the software's suitability as regards their    *
 *  requirements in conditions enabling the security of their systems and/or   *
 *  data to be ensured and,  more generally, to use and operate it in the      *
 *  same conditions as regards security.                                       *
 *                                                                             *
 *  The fact that you are presently reading this means that you have had       *
 *  knowledge of the CeCILL license and that you accept its terms.             *
 *                                                                             *
 ******************************************************************************/

#ifndef M_DB_H

#define M_DB_H

#ifdef _ENABLE_DB

#include "askl.h"
#include "m_string.h"
#include "m_queue.h"
#include "util/m_util_vfprintf.h"

/** @defgroup db module::db */

typedef struct m_db {
    pthread_mutex_t *_lock;
    int _driver;
    void *_con;
} m_db;

/**
 * @ingroup db
 * @struct m_db
 *
 * This structure holds the database engine dependant connection handle and
 * a lock. This structure is used thoughout the Concrete database access
 * API as a mean to be independant from the underlying database engine and
 * to ensure the thread safety of the various functions.
 *
 * The fields of this structure are private.
 *
 * @b private @ref _lock a mutex to ensure safe concurrency
 * @b private @ref _driver the database driver to use
 * @b private @ref _con database engine dependant connection handle
 *
 */

typedef struct m_dbpool {
    m_db *_safe;
    m_queue *_pool;
} m_dbpool;

/**
 * @ingroup db
 * @struct m_dbpool
 *
 * This structure contains a pool of connections, implemented using a queue,
 * and a "safe" connection.
 *
 * If the pool is empty, the safe connection will be provided, which can be
 * a bit slower due to locking contention.
 *
 */

#define DB_DRIVER_CACHE 0   /* not a real driver; marshalled recordset */
#define DB_DRIVER_MYSQL 1   /* MySQL native driver */
#define DB_DRIVER_PGSQL 2   /* PostgreSQL native driver */
#define DB_DRIVER_DODBC 3   /* Direct ODBC driver */
#define DB_DRIVER_SLITE 4   /* SQLite native driver */

#define DB_DRIVER_MAX   4   /* total number of drivers */

typedef struct m_recordset {
    unsigned int _driver;
    unsigned int id;
    unsigned int rows;
    unsigned int cols;
    unsigned int current_row;
    unsigned int _last_row;
    void *_res;
} m_recordset;

/**
 * @ingroup db
 * @struct m_recordset
 * 
 * This structure is designed to allow an easy access to an SQL query
 * results.
 *
 * A recordset is a collection of rows: by default, it will be set to
 * the first row (row 0).
 *
 * The recordset can then move to the next row using @ref db_move()
 * or @ref db_movenext(). It can also move back to a previous record
 * with @ref db_move() or @ref db_moveprev().
 *
 * Once the recordset is set to the row of interest, data can be accessed
 * using the column name. @ref db_field() allow data type independant
 * access, while various wrappers like @ref db_integer returns the data
 * with native C types.
 *
 * This structure exposes four public fields:
 * @ref id is the id which was assigned to the record if inserted
 *         with an autoincrement index
 * @ref rows is the number of rows stored in the recordset
 * @ref cols is the number of columns
 * @ref current_row is the current row number
 *
 * The remaining fields are private.
 *
 * @b private @ref _driver is the type of recordset, to use the proper driver
 * @b private @ref _last_row is the number of the last row accessed,
 *                           which is used for internal caching.
 * @b private @ref _res is the database engine dependant data storage
 * structure
 *
 */

/* -------------------------------------------------------------------------- */

public m_db *db_open(int drv, const char *user, const char *pwd, const char *db,
                     const char *host, unsigned short port, int npflags);

/**
 * @ingroup db
 * @fn m_db *db_open(int drv, const char *user, const char *pwd, const char *db,
                     const char *host, const char *port, int npflags)
 * @param drv the database driver to use
 * @param user username
 * @param pwd user password
 * @param db database name
 * @param host hostname or ip address of the server hosting the database
 * @param port network port on which the database server is listening
 * @param npflags database engine specific flags
 * @return a new connection to the database, or NULL if an error occured
 *
 * This function creates a new connection to the specified database.
 * The host, port and npflags parameters will usually be left to NULL.
 *
 */

/* -------------------------------------------------------------------------- */

public m_recordset *db_query(m_db *con, const char *query, ...);

/**
 * @ingroup db
 * @fn m_recordset *db_query(m_db *con, const char *query, ...)
 * @param con a database connection
 * @param query a format string which will generate the SQL query
 * @param ... various parameters matching the format string
 * @return a pointer to a new recordset, or NULL if an error occurs
 *
 * This function will generate a safe SQL query from the format string then
 * send it through the database connection. The results are stored in
 * a recordset structure, which can be then manipulated using other functions
 * like @ref db_move() to select a record or @ref db_data() to  get the value
 * of some specific field in the current record.
 *
 * The resulting recordset pointer should be free'd using @ref db_free().
 *
 * If the request does not generate a result, the recordset will be empty,
 * but the number of affected rows will be recorded in the @ref rows field,
 * and the newly inserted index (if the object has autoincrement enabled) will
 * be returned in the @ref id field of the recordset.
 *
 * You can check if the request generated results by calling @ref db_eof()
 * on the recordset. If the function returns 1, then the recordset is empty.
 *
 * @warning: if you use the MySQL driver, and call a stored procedure, this
 * function will not be able to get the last inserted id even if the call
 * is successful. It is then recommended that you use the LAST_INSERT_ID()
 * SQL function to get it.
 *
 * @see db_data(), db_free()
 *
 */

/* -------------------------------------------------------------------------- */

public int db_exec(m_db *con, const char *query, ...);

/**
 * @ingroup db
 * @fn m_recordset *db_query(m_db *con, const char *query, ...)
 * @param con a database connection
 * @param query a format string which will generate the SQL query
 * @param ... various parameters matching the format string
 * @return 0 if all went fine, -1 otherwise
 *
 * This function is a wrapper to db_query which, for convenience, does not
 * return a recordset, but simply 0 if the query succeeded or -1 if an
 * error occured.
 *
 * @see db_query()
 *
 */

/* -------------------------------------------------------------------------- */

public int db_move(m_recordset *r, unsigned int row);

/**
 * @ingroup db
 * @fn int db_move(m_recordset *r, unsigned int row)
 * @param r a recordset
 * @param row a row number
 * @return the row number or -1 if an error occured
 *
 * This function moves the recordset to the given row.
 *
 */

/* -------------------------------------------------------------------------- */

public int db_eof(m_recordset *r);

/**
 * @ingroup db
 * @fn int db_eof(m_recordset *r)
 * @param r a recordset
 * @return 1 if the recordset can not be forwarded, 0 otherwise
 *
 * This function tests if it is possible to move next in a recordset.
 *
 */

/* -------------------------------------------------------------------------- */

public int db_movefirst(m_recordset *r);

/**
 * @ingroup db
 * @fn int db_movefirst(m_recordset *r)
 * @param r a recordset
 * @return the row number or -1 if an error occured
 *
 * This function rewinds the given recordset to its first row.
 *
 * @see db_move()
 *
 */

/* -------------------------------------------------------------------------- */

public int db_movelast(m_recordset *r);

/**
 * @ingroup db
 * @fn int db_movelast(m_recordset *r)
 * @param r a recordset
 * @return the row number or -1 if an error occured
 *
 * This function forwards the given recordset to its last row.
 *
 * @see db_move()
 *
 */

/* -------------------------------------------------------------------------- */

public int db_moveprev(m_recordset *r);

/**
 * @ingroup db
 * @fn int db_moveprev(m_recordset *r)
 * @param r a recordset
 * @return the row number or -1 if an error occured
 *
 * This function rewinds the given recordset to the previous row.
 *
 * @see db_move()
 *
 */

/* -------------------------------------------------------------------------- */

public int db_movenext(m_recordset *r);

/**
 * @ingroup db
 * @fn int db_movenext(m_recordset *r)
 * @param r a recordset
 * @return the row number or -1 if an error occured
 *
 * This function forwards the given recordset to the next row.
 *
 * @see db_move()
 *
 */

/* -------------------------------------------------------------------------- */

public const char *db_field(m_recordset *r, const char *column, size_t *l);

/**
 * @ingroup db
 * @fn const char *db_field(m_recordset *r, const char *column, size_t *l)
 * @param r a recordset
 * @param column a column name
 * @param l an optional pointer to an unsigned int
 * @return the value of the data field or NULL
 *
 * This function returns the raw data from the given column at the current row
 * of the recordset given in parameter. If @ref l is a valid pointer, the size
 * of the returned data will be written in it.
 *
 * The returned data are read-only and thus must not be freed nor modified.
 *
 */

/* -------------------------------------------------------------------------- */

public m_string *db_varchar(m_recordset *r, const char *column);

/**
 * @ingroup db
 * @fn m_string *db_varchar(m_recordset *r, const char *column)
 * @param r a recordset
 * @param column a column name
 * @return the value of the data field in a new m_string, or NULL
 *
 * This function returns the raw data from the given column, encapsulated
 * in a m_string structure. Don't forget to destroy the @ref m_string
 * after use with @ref string_free().
 *
 */

/* -------------------------------------------------------------------------- */

public long db_integer(m_recordset *r, const char *column);

/**
 * @ingroup db
 * @fn long db_integer(m_recordset *r, const char *column)
 * @param r a recordset
 * @param column a column name
 * @return the value of the integer field
 *
 * This function returns the integral value of the given column.
 *
 */

/* -------------------------------------------------------------------------- */

public double db_double(m_recordset *r, const char *column);

/**
 * @ingroup db
 * @fn double db_double(m_recordset *r, const char *column)
 * @param r a recordset
 * @param column a column name
 * @return the value of the float field
 *
 * This function returns the floating point value of the given column.
 *
 */

/* -------------------------------------------------------------------------- */

public m_recordset *db_free(m_recordset *r);

/**
 * @ingroup db
 * @fn m_recordset *db_free(m_recordset *r)
 * @param r a recordset
 * @return NULL
 *
 * This function destroys a recordset.
 *
 */

/* -------------------------------------------------------------------------- */

public m_db *db_close(m_db *connection);

/**
 * @ingroup db
 * @fn m_db *db_close(m_db connection)
 * @param connection a database connection
 * @return NULL
 *
 * This function closes and cleans up the given database connection.
 *
 */

/* -------------------------------------------------------------------------- */

public m_dbpool *dbpool_open(int drv, unsigned int n, const char *user,
                             const char *pwd, const char *db,
                             const char *host, unsigned short port,
                             int npflags);

/**
 * @ingroup db
 * @fn m_dbpool *dbpool_open(int drv, unsigned int n, const char *user,
 *                           const char *pwd, const char *db,
 *                           const char *host, unsigned short port,
 *                           int npflags)
 * @param drv the database driver to use
 * @param n the number of connections in the pool
 * @param user username
 * @param pwd user password
 * @param db database name
 * @param host hostname or ip address of the server hosting the database
 * @param port network port on which the database server is listening
 * @param npflags database engine specific flags
 * @return a new pool of connections to the database, NULL if an error occured
 *
 * This function will allocate a new pool of database connections and fill it
 * with exactly @ref n connections, ready to be used.
 *
 * Connections can be borrowed from the pool with @ref dbpool_borrow() and
 * must be returned after use by calling @ref dbpool_return().
 * Failure to put back a borrowed connection in the pool will result in
 * memory leaks.
 *
 * When the database connections pool is not needed anymore, you can destroy
 * it and all the connections it contains by calling @ref dbpool_close().
 * Once again, be careful to put back all the connections in the pool if you
 * want them to be automatically destroyed this way.
 *
 */

/* -------------------------------------------------------------------------- */

public m_db *dbpool_borrow(m_dbpool *pool);

/**
 * @ingroup db
 * @fn m_db *dbpool_borrow(m_dbpool *pool)
 * @param pool a database connections pool
 * @return a database connection, or NULL if an error occured
 *
 * This function will return a connection from the given pool. Connections
 * are returned in the same order they were stored in the pool.
 *
 * A connection borrowed from the pool should be returned using
 * @ref dbpool_return() when it is no longer used.
 *
 */

/* -------------------------------------------------------------------------- */

public m_db *dbpool_return(m_dbpool *pool, m_db *connection);

/**
 * @ingroup db
 * @fn m_db *dbpool_return(m_dbpool *pool, m_db *connection)
 * @param pool a database connections pool
 * @param connection a database connection
 * @return NULL
 *
 * This function put back the given database connection in the given pool. You
 * can put a database connection in a pool to which it didn't belonged in the
 * first place without problem, but it is not advised to do so.
 *
 * The database connections pool works as a FIFO queue, so in the event you
 * were to manually store connections to different databases in the same pool,
 * you can get them back with dbpool_borrow in the same order you stored them.
 *
 * This function always return NULL, so you can use it to clear the reference
 * to the connection you are putting back in the pool, like this:
 * con = dbpool_return(con); // con is now NULL
 *
 */

/* -------------------------------------------------------------------------- */

public m_dbpool *dbpool_close(m_dbpool *pool);

/**
 * @ingroup db
 * @fn m_dbpool *dbpool_close(m_dbpool *pool)
 * @param pool a database connections pool
 * @return NULL
 *
 * This function destroys the given connections pool and all the connections
 * it contains. It always returns NULL.
 *
 */

/* -------------------------------------------------------------------------- */

/* _ENABLE_DB */
#endif

#endif
