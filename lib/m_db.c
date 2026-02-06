/*******************************************************************************
 *  Concrete Server                                                            *
 *  Copyright (c) 2005-2019 Raphael Prevost <raph@el.bzh>                      *
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

#include "m_db.h"

/* -------------------------------------------------------------------------- */
#ifdef _ENABLE_DB
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
#ifdef _ENABLE_MYSQL    /* MySQL Database Driver */
/* -------------------------------------------------------------------------- */

static void *_db_open_mysql(const char *user, const char *pwd, const char *db,
                            const char *host, unsigned short port, int npflags)
{
    MYSQL *c = NULL;

    int enabled = 1; /* XXX my_bool type has been removed by MySQL */

    if (! (c = mysql_init(NULL)) ) {
        fprintf(stderr, "db_open(): mysql_init() failed!\n");
        return NULL;
    }

    #if (MYSQL_VERSION_ID > 50002)
    /* enable automatic reconnection (disabled by default MySQL >=5.0.3) */
    mysql_options(c, MYSQL_OPT_RECONNECT, & enabled);
    #endif

    /* enable multiresults to handle CALL */
    npflags |= CLIENT_MULTI_RESULTS;

    if (! mysql_real_connect(c, host, user, pwd, db, port, NULL, npflags)) {
        fprintf(
            stderr,
            ERR(db_open, mysql_real_connect)": %s\n",
            mysql_error(c)
        );
        mysql_close(c);
        return NULL;
    }

    #if (MYSQL_VERSION_ID > 50002 && MYSQL_VERSION_ID < 50019)
    /* MySQL up to version 5.0.19 incorrectly reset the option */
    mysql_options(c, MYSQL_OPT_RECONNECT, & enabled);
    #endif

    return c;
}

/* -------------------------------------------------------------------------- */

static int __db_sql_mysql(m_db *con, m_recordset *r, const char *query_text,
                          size_t query_size)
{
    MYSQL_RES *set = NULL;

    /* clean spurious results due to a previous procedure call */
    while (mysql_more_results(con->_con)) {
        mysql_free_result(mysql_use_result(con->_con));
        if (mysql_next_result(con->_con) > 0) {
            fprintf(
                stderr,
                ERR(_db_sql, mysql_next_result)": %s\n",
                mysql_error(con->_con)
            );
            return -1;
        }
    }

    /* perform the query */
    if (mysql_real_query(con->_con, query_text, query_size)) {
        fprintf(
            stderr,
            ERR(_db_sql, mysql_query)": %s\n",
            mysql_error(con->_con)
        );
        return -1;
    }

    /* fetch the result */
    if (! (set = mysql_store_result(con->_con)) ) {
        /* the query does not generate results, or an error occured */
        if (mysql_errno(con->_con)) {
            fprintf(
                stderr,
                ERR(_db_sql, mysql_store_result)": %s\n",
                mysql_error(con->_con)
            );
            return -1;
        }
    }

    /* get the last insert id for autoincrement */
    r->id = mysql_insert_id(con->_con);
    r->rows = (set) ? mysql_num_rows(set) : mysql_affected_rows(con->_con);
    r->cols = (set) ? mysql_num_fields(set) : 0;
    r->current_row = 0; r->_last_row = 0;
    r->_res = set;

    return 0;
}

/* -------------------------------------------------------------------------- */

static void _db_move_mysql(m_recordset *r, unsigned int row)
{
    mysql_data_seek(r->_res, row);
}

/* -------------------------------------------------------------------------- */

static const char *_db_field_mysql(m_recordset *r, const char *column,
                                   size_t *l)
{
    unsigned int i = 0;
    MYSQL_ROW row = NULL;
    MYSQL_FIELD *fields = NULL;
    unsigned long *len = NULL;

    if (! (fields = mysql_fetch_fields(r->_res)) ) {
        debug("db_field(): cannot retrieve fields informations.\n");
        return NULL;
    }

    /* avoid fetching the same data twice */
    if (r->_last_row != r->current_row) {
        /* coerce MySQL to use our recordset model */
        mysql_data_seek(r->_res, r->current_row);
    } else row = ((MYSQL_RES *) r->_res)->current_row;

    r->_last_row = r->current_row;

    /* fetch data if necessary */
    if (! row && ! (row = mysql_fetch_row(r->_res)) ) {
        debug("db_field(): cannot fetch row.\n");
        return NULL;
    }

    if (! (len = mysql_fetch_lengths(r->_res)) ) {
        debug("db_field(): cannot retrieve fields length.\n");
        return NULL;
    }

    /* MySQL doesn't know how to get a column number from its name */
    for (i = 0; i < r->cols; i ++) {
        if (! strcmp(fields[i].name, column)) {
            if (l) *l = len[i];
            return row[i];
        }
    }

    return NULL;
}

/* -------------------------------------------------------------------------- */

static void _db_free_mysql(m_recordset *r)
{
    mysql_free_result(r->_res);
}

/* -------------------------------------------------------------------------- */

static void _db_close_mysql(m_db *connection)
{
    mysql_close(connection->_con);
}

/* -------------------------------------------------------------------------- */
#endif /* _ENABLE_MYSQL */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
#ifdef _ENABLE_SQLITE    /* SQLite Database Driver */
/* -------------------------------------------------------------------------- */

static void *_db_open_sqlite(UNUSED const char *user, UNUSED const char *pwd,
                             const char *db, UNUSED const char *host,
                             UNUSED unsigned short port, UNUSED int npflags)
{
    sqlite3 *con = NULL;

    if (sqlite3_open(db, & con)) {
        fprintf(stderr, "db_open(): sqlite3_open() failed!\n");
        return NULL;
    }

    return con;
}

/* -------------------------------------------------------------------------- */

static int __db_sql_sqlite(m_db *con, m_recordset *r, const char *query_text,
                           UNUSED size_t query_size)
{
    char **res = NULL, *err = NULL;
    int ret = 0;

    ret = sqlite3_get_table(
        con->_con,
        query_text,
        & res,
        (int *) & r->rows,
        (int *) & r->cols,
        & err
    );

    if (ret != SQLITE_OK) {
        fprintf(
            stderr,
            ERR(_db_sql, sqlite3_get_table)": %s\n",
            err
        );
        sqlite3_free(err);
        return -1;
    }

    r->id = sqlite3_last_insert_rowid(con->_con);
    r->current_row = 0; r->_last_row = 0;
    r->rows = (res) ? r->rows - 1 : (unsigned) sqlite3_changes(con->_con);
    r->_res = res;

    return 0;
}

/* -------------------------------------------------------------------------- */

static void _db_move_sqlite(UNUSED m_recordset *r, UNUSED unsigned int row)
{
    return;
}

/* -------------------------------------------------------------------------- */

static const char *_db_field_sqlite(m_recordset *r, const char *column,
                                    size_t *l)
{
    unsigned int i = 0;
    int index = -1;
    const char **set = r->_res;
    const char *ret = NULL;

    /* get the column index */
    for (i = 0; i < r->cols; i ++) {
        if (! strcmp(set[i], column)) { index = i; break; }
    }

    if (index == -1) return NULL;

    /* get the row */
    if ( (ret = set[(r->current_row + 1) * r->cols + index]) && l)
        *l = strlen(ret);

    return ret;
}

/* -------------------------------------------------------------------------- */

static void _db_free_sqlite(m_recordset *r)
{
    sqlite3_free_table(r->_res);
}

/* -------------------------------------------------------------------------- */

static void _db_close_sqlite(m_db *connection)
{
    sqlite3_close(connection->_con);
}

/* -------------------------------------------------------------------------- */
#endif /* _ENABLE_SQLITE */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* Drivers vectors */
/* -------------------------------------------------------------------------- */

/* OPEN */
static void *(*_db_open[5])(const char *, const char *, const char *,
                            const char *, unsigned short, int) = {
    NULL, /* marshalled recordset */

    #ifdef _ENABLE_MYSQL
    _db_open_mysql,
    #else
    NULL,
    #endif

    #ifdef _ENABLE_PGSQL
    _db_open_pgsql,
    #else
    NULL,
    #endif

    #ifdef _ENABLE_DODBC
    _db_open_dodbc,
    #else
    NULL,
    #endif

    #ifdef _ENABLE_SQLITE
    _db_open_sqlite,
    #else
    NULL,
    #endif
};

/* QUERY */
static int (*__db_sql[5])(m_db *, m_recordset *, const char *, size_t) = {
    NULL, /* marshalled recordset */

    #ifdef _ENABLE_MYSQL
    __db_sql_mysql,
    #else
    NULL,
    #endif

    #ifdef _ENABLE_PGSQL
    __db_sql_pgsql,
    #else
    NULL,
    #endif

    #ifdef _ENABLE_DODBC
    __db_sql_dodbc,
    #else
    NULL,
    #endif

    #ifdef _ENABLE_SQLITE
    __db_sql_sqlite,
    #else
    NULL,
    #endif
};

/* MOVE */
static void (*_db_move[5])(m_recordset *, unsigned int) = {
    NULL, /* marshalled recordset */

    #ifdef _ENABLE_MYSQL
    _db_move_mysql,
    #else
    NULL,
    #endif

    #ifdef _ENABLE_PGSQL
    _db_move_pgsql,
    #else
    NULL,
    #endif

    #ifdef _ENABLE_DODBC
    _db_move_dodbc,
    #else
    NULL,
    #endif

    #ifdef _ENABLE_SQLITE
    _db_move_sqlite,
    #else
    NULL,
    #endif
};

/* FIELD */
static const char *(*_db_field[5])(m_recordset *, const char *, size_t *) = {
    NULL, /* marshalled recordset */

    #ifdef _ENABLE_MYSQL
    _db_field_mysql,
    #else
    NULL,
    #endif

    #ifdef _ENABLE_PGSQL
    _db_field_pgsql,
    #else
    NULL,
    #endif

    #ifdef _ENABLE_DODBC
    _db_field_dodbc,
    #else
    NULL,
    #endif

    #ifdef _ENABLE_SQLITE
    _db_field_sqlite,
    #else
    NULL,
    #endif
};

/* FREE */
static void (*_db_free[5])(m_recordset *) = {
    NULL, /* marshalled recordset */

    #ifdef _ENABLE_MYSQL
    _db_free_mysql,
    #else
    NULL,
    #endif

    #ifdef _ENABLE_PGSQL
    _db_free_pgsql,
    #else
    NULL,
    #endif

    #ifdef _ENABLE_DODBC
    _db_free_dodbc,
    #else
    NULL,
    #endif

    #ifdef _ENABLE_SQLITE
    _db_free_sqlite,
    #else
    NULL,
    #endif
};

/* CLOSE */
static void (*_db_close[5])(m_db *) = {
    NULL, /* marshalled recordset */

    #ifdef _ENABLE_MYSQL
    _db_close_mysql,
    #else
    NULL,
    #endif

    #ifdef _ENABLE_PGSQL
    _db_close_pgsql,
    #else
    NULL,
    #endif

    #ifdef _ENABLE_DODBC
    _db_close_dodbc,
    #else
    NULL,
    #endif

    #ifdef _ENABLE_SQLITE
    _db_close_sqlite,
    #else
    NULL,
    #endif
};

/* -------------------------------------------------------------------------- */
/* Generic functions */
/* -------------------------------------------------------------------------- */

public m_db *db_open(int drv, const char *user, const char *pwd, const char *db,
                     const char *host, unsigned short port, int npflags)
{
    m_db *ret = NULL;

    if (drv < 0 || drv > DB_DRIVER_MAX || ! _db_open[drv]) {
        fprintf(stderr, "db_open(): no such driver!\n");
        return NULL;
    }

    if (! (ret = malloc(sizeof(*ret))) ) {
        perror(ERR(db_open, malloc));
        return NULL;
    }

    if (! (ret->_lock = malloc(sizeof(*ret->_lock))) ) {
        perror(ERR(db_open, malloc));
        goto _err_init;
    }

    if (pthread_mutex_init(ret->_lock, NULL) == -1) {
        perror(ERR(db_open, pthread_mutex_init));
        free(ret->_lock);
        goto _err_init;
    }

    if (! (ret->_con = _db_open[drv](user, pwd, db, host, port, npflags)) )
        goto _err_con_init;

    ret->_driver = drv;

    return ret;

_err_con_init:
    pthread_mutex_destroy(ret->_lock);
    free(ret->_lock);
_err_init:
    free(ret);
    return NULL;
}

/* -------------------------------------------------------------------------- */

static m_recordset *_db_sql(m_db *con, const char *query, va_list params)
{
    char *query_text = NULL;
    size_t query_size = 0;
    m_recordset *r = NULL;

    if (! con || ! query) {
        debug("_db_sql(): bad parameters.\n");
        return NULL;
    }

    /* prepare the query and store it */
    query_size = m_vsnprintf_db(con->_con, NULL, 0, query, params);
    if (! (query_text = malloc((query_size + 1) * sizeof(*query_text))) ) {
        perror(ERR(_db_sql, malloc));
        return NULL;
    }
    m_vsnprintf_db(con->_con, query_text, query_size, query, params);

    if (! (r = malloc(sizeof(*r))) ) {
        perror(ERR(_db_sql, malloc));
        goto _err_alloc;
    }

    #ifdef DEBUG_SQL
    fprintf(
        stderr,
        "_db_sql(): processing the request: %.*s\n",
        (int) query_size, query_text
    );
    #endif

    /* ensure atomicity */
    pthread_mutex_lock(con->_lock);

    /* process the query */
    if (__db_sql[con->_driver](con, r, query_text, query_size) == -1)
        goto _err_query;

    pthread_mutex_unlock(con->_lock);

    /* the recordset inherit the database driver */
    r->_driver = con->_driver;

    /* ensure the recordset is set to the first row */
    if (r && r->_res) db_movefirst(r);

_err_alloc:
    free(query_text);
    return r;

_err_query:
    pthread_mutex_unlock(con->_lock);
    free(query_text); free(r);
    return NULL;
}

/* -------------------------------------------------------------------------- */

public m_recordset *db_query(m_db *con, const char *query, ...)
{
    va_list params;
    m_recordset *r = NULL;

    if (! con || ! query) {
        debug("db_query(): bad parameters.\n");
        return NULL;
    }

    va_start(params, query);
    r = _db_sql(con, query, params);
    va_end(params);

    return r;
}

/* -------------------------------------------------------------------------- */

public int db_exec(m_db *con, const char *query, ...)
{
    va_list params;
    m_recordset *r = NULL;
    int ret = 0;

    if (! con || ! query) {
        debug("db_exec(): bad parameters.\n");
        return 0;
    }

    va_start(params, query);
    r = _db_sql(con, query, params);
    va_end(params);

    ret = (r) ? 0 : -1;

    r = db_free(r);

    return ret;
}

/* -------------------------------------------------------------------------- */

public int db_move(m_recordset *r, unsigned int row)
{
    if (! r || ! r->_res || row >= r->rows) {
        /* XXX no debug as this error condition can be used to catch EOF */
        return -1;
    }

    _db_move[r->_driver](r, row);

    r->current_row = row;

    return row;
}

/* -------------------------------------------------------------------------- */

public int db_eof(m_recordset *r)
{
    return (! r || ! r->_res || r->current_row + 1 >= r->rows) ? 1 : 0;
}

/* -------------------------------------------------------------------------- */

public int db_movefirst(m_recordset *r)
{
    return db_move(r, 0);
}

/* -------------------------------------------------------------------------- */

public int db_movelast(m_recordset *r)
{
    if (! r || ! r->_res) {
        debug("db_movelast(): bad parameters.\n");
        return -1;
    }

    return db_move(r, r->rows - 1);
}

/* -------------------------------------------------------------------------- */

public int db_moveprev(m_recordset *r)
{
    if (! r || ! r->_res) {
        debug("db_moveprev(): bad parameters.\n");
        return -1;
    }

    return db_move(r, r->current_row - 1);
}

/* -------------------------------------------------------------------------- */

public int db_movenext(m_recordset *r)
{
    if (! r || ! r->_res) {
        debug("db_movenext(): bad parameters.\n");
        return -1;
    }

    return db_move(r, r->current_row + 1);
}

/* -------------------------------------------------------------------------- */

public const char *db_field(m_recordset *r, const char *column, size_t *l)
{
    if (! r || ! r->_res || ! column) {
        debug("db_field(): bad parameters.\n");
        return NULL;
    }

    return _db_field[r->_driver](r, column, l);
}

/* -------------------------------------------------------------------------- */

public m_string *db_varchar(m_recordset *r, const char *column)
{
    const char *field = NULL;
    size_t fieldlen = 0;
    m_string *data = NULL;

    if ( (field = db_field(r, column, & fieldlen)) ) {
        data =  string_alloc(field, fieldlen);
    }

    return data;
}

/* -------------------------------------------------------------------------- */

public long db_integer(m_recordset *r, const char *column)
{
    const char *field = NULL;
    long data = 0;

    if ( (field = db_field(r, column, NULL)) ) {
        data = atoi(field);
    }

    return data;
}

/* -------------------------------------------------------------------------- */

public double db_double(m_recordset *r, const char *column)
{
    const char *field = NULL;
    double data = 0;

    if ( (field = db_field(r, column, NULL)) ) {
        data = atof(field);
    }

    return data;
}

/* -------------------------------------------------------------------------- */

public m_recordset *db_free(m_recordset *r)
{
    if (r) { _db_free[r->_driver](r); free(r); }

    return NULL;
}

/* -------------------------------------------------------------------------- */

public m_db *db_close(m_db *connection)
{
    if (connection) {
        _db_close[connection->_driver](connection);
        pthread_mutex_destroy(connection->_lock);
        free(connection->_lock); free(connection);
    }

    return NULL;
}

/* -------------------------------------------------------------------------- */
/* Pool functions */
/* -------------------------------------------------------------------------- */

public m_dbpool *dbpool_open(int drv, unsigned int n, const char *user,
                             const char *pwd, const char *db,
                             const char *host, unsigned short port,
                             int npflags)
{
    m_db *connection = NULL;
    m_dbpool *pool = NULL;
    int ret = 0;

    if (! (pool = malloc(sizeof(*pool))) ) {
        perror(ERR(dbpool_open, malloc));
        return NULL;
    }

    /* try to open the "safe" connection */
    if (! (pool->_safe = db_open(drv, user, pwd, db, host, port, npflags)) ) {
        debug("dbpool_open(): connection failed.\n");
        free(pool);
        return NULL;
    }

    /* create and fill the pool */
    if (! (pool->_pool = queue_alloc()) ) {
        debug("dbpool_open(): cannot allocate the pool.\n");
        pool->_safe = db_close(pool->_safe);
        free(pool);
        return NULL;
    }

    do {
        connection = db_open(drv, user, pwd, db, host, port, npflags);
        ret = queue_enqueue(pool->_pool, connection);
    } while (n -- && connection && ret != -1);

    if (ret == -1 || ! connection) {
        debug("dbpool_open(): error filling the pool.\n");
        db_close(pool->_safe);
        queue_free_nodes(pool->_pool, (void *(*)(void *)) db_close);
        pool->_pool = queue_free(pool->_pool);
        free(pool);
        return NULL;
    }

    return pool;
}

/* -------------------------------------------------------------------------- */

public m_db *dbpool_borrow(m_dbpool *pool)
{
    m_db *con = NULL;

    if (! pool) {
        debug("dbpool_borrow(): bad parameters.\n");
        return NULL;
    }

    if (! (con = queue_pop(pool->_pool)) )
        return pool->_safe;

    return con;
}

/* -------------------------------------------------------------------------- */

public m_db *dbpool_return(m_dbpool *pool, m_db *connection)
{
    if (! pool || ! connection) return NULL;

    if (connection == pool->_safe) return NULL;

    if (queue_enqueue(pool->_pool, connection) == -1) {
        debug("dbpool_return(): failed to return the connection!\n");
        return NULL;
    }

    return NULL;
}

/* -------------------------------------------------------------------------- */

public m_dbpool *dbpool_close(m_dbpool *pool)
{
    if (! pool) return NULL;

    /* destroy the connections pool and the safe connection */
    queue_free_nodes(pool->_pool, (void *(*)(void *)) db_close);
    queue_free(pool->_pool);
    db_close(pool->_safe);

    free(pool);

    return NULL;
}

/* -------------------------------------------------------------------------- */
#else
/* -------------------------------------------------------------------------- */

/* Database support will not be compiled in the Mammouth Library */
#ifdef __GNUC__
__attribute__ ((unused)) static int __dummy__ = 0;
#endif

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */
