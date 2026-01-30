/*******************************************************************************
 *  ASKL.                                                                      *
 *  Copyright (c) 2026 Raphael Prevost <raph@el.bzh>                           *
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

#ifndef M_CONFIG_H

#define M_CONFIG_H

#if defined(_ENABLE_CONFIG) && defined(HAS_LIBXML)

#include "askl.h"
#include "askl_server.h"
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/valid.h>

#if  defined(_ENABLE_DB) || defined(_ENABLE_SSL)
#ifndef _ENABLE_HASHTABLE
    #error "ASKL: the configuration module requires the builtin hashtable."
#endif
#ifdef _ENABLE_DB
#include "m_db.h"
#endif
#endif

/** @defgroup config module::config */

#define CONFIG_PROFILE_PRD 0x01     /* production */
#define CONFIG_PROFILE_DBG 0x02     /* debug */
#define CONFIG_PROFILE_ANY 0x03     /* any (default) */

/* -------------------------------------------------------------------------- */

INTERNAL int configure(const char *path, const char *configfile);

/**
 * @ingroup config
 * @fn int configure(const char *path, const char *configfile)
 * @param path the directory where the configuration file can be found
 * @param configfile the name of the configuration file
 * @return 0 if everything went fine, -1 otherwise
 *
 * This function loads the naer XML configuration file, validates it
 * against the DTD and applies the configuration.
 *
 * This function is private and should only be called by Concrete.
 *
 */

/* -------------------------------------------------------------------------- */

ASKL_API void config_force_profile(int profile);

/* -------------------------------------------------------------------------- */

INTERNAL void configure_cleanup(void);

/**
 * @ingroup config
 * @fn void configure_cleanup(void)
 *
 * Cleanup all resources used by the configuration module.
 *
 */

/* -------------------------------------------------------------------------- */

ASKL_API unsigned int config_get_concurrency(void);

/**
 * @ingroup config
 * @fn unsigned int config_get_concurrency(void)
 * @return the number of worker threads the server must use
 *
 */

/* -------------------------------------------------------------------------- */

#ifdef _ENABLE_DB
public m_dbpool *config_get_db(const char *id);

/**
 * @ingroup config
 * @fn m_dbpool *config_get_db(const char *id)
 * @param id a database connections pool identifier
 * @return a pointer to the pool matching the identifier, or NULL
 *
 */
#endif

/* -------------------------------------------------------------------------- */

#ifdef _ENABLE_SSL

INTERNAL SSL_CTX *config_get_ssl(unsigned int id);

/**
 * @ingroup config
 * @fn m_dbpool *config_get_ssl(unsigned int id)
 * @param id a plugin identifier
 * @return a pointer to the matching SSL_CTX, or NULL
 *
 */
#endif

/* -------------------------------------------------------------------------- */

INTERNAL int config_process(const char *config, size_t len);

/**
 * @ingroup config
 * @fn int config_process(const char *config, size_t len)
 * @param config the content of the XML configuration file
 * @param len the length of the content
 * @return 0 if everything went fine, -1 otherwise
 *
 * This callback actually process the configuration file content, loaded
 * by @ref configure().
 *
 */

/* -------------------------------------------------------------------------- */

/* _ENABLE_XML && HAS_LIBXML */
#endif

#endif
