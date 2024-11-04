/*******************************************************************************
 *  Concrete Server                                                            *
 *  Copyright (c) 2005-2024 Raphael Prevost <raph@el.bzh>                      *
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

#ifndef M_VALUE_H

#define M_VALUE_H

#include "m_core_def.h"

/** @defgroup value core::value */

#if UINTPTR_MAX == 0xffffffff
typedef struct m_value {
    union {
        void *pointer;
        uintptr_t integer;
        float decimal; /* assume IEEE754 32-bits single precision format */
    } data;
    uint16_t type;
} m_value;
#elif UINTPTR_MAX == 0xffffffffffffffff
#pragma pack(push, 1)
typedef struct m_value {
    union {
        void *pointer;
        uintptr_t integer;
        double decimal; /* assume IEEE754 64-bits double precision format */
    } data;
    uint16_t type;
} m_value;
#pragma pack(pop)
#else
#error Unsupported platform
#endif

#define VALUE_POINTER 0x0
#define VALUE_INTEGER 0x1
#define VALUE_DECIMAL 0x2

#define VALUE_BOOL   0x10
#define VALUE_TRUE   0x20
#define VALUE_NULL   0x40

/**
 * @ingroup value
 * @struct m_value
 *
 * This structure allows some level of type-checking within the included data
 * structures such as @ref m_trie or m_hashtable.
 * 
 */

/* -------------------------------------------------------------------------- */

#endif
