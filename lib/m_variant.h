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

#ifndef M_VARIANT_H

#define M_VARIANT_H

#include "m_core_def.h"
#include "m_string.h"

/** @defgroup variant core::variant */

typedef struct variant {
    union {
        void *pointer;
        uint64_t integer;
        double decimal;     /* assume IEEE754 64-bits double precision format */
    } value;
    union {
        struct {
            uint32_t dword;
            uint16_t word;
            uint8_t  byte;
            uint8_t  type;  /* reserved unless type & _VALUE_OBJECT */
        } fields;
        struct {
            uint8_t bytes[7];
            uint8_t type;
        } raw;
    } metadata;             /* 56 bits of reclaimed padding to store metadata */
} variant;

#define VALUE_NULL        0
#define VALUE_STRING      1
#define VALUE_INTEGER     2
#define VALUE_BOOLEAN     3
#define VALUE_DECIMAL     4
#define VALUE_POINTER     5
#define _VALUE_OBJECT  0x80

#define is_string(v) ((v).metadata.fields.type == VALUE_STRING)
#define is_integer(v) ((v).metadata.fields.type == VALUE_INTEGER)
#define is_boolean(v) ((v).metadata.fields.type == VALUE_BOOLEAN)
#define is_decimal(v) ((v).metadata.fields.type == VALUE_DECIMAL)
#define is_pointer(v) ((v).metadata.fields.type == VALUE_POINTER)
#define _is_object(v) ((v).metadata.fields.type & _VALUE_OBJECT)

/**
 * @ingroup variant
 * @struct variant
 *
 * This structure allows some level of type-checking within the included data
 * structures such as @ref m_trie or m_hashtable.
 * 
 */

/* -------------------------------------------------------------------------- */

public variant CALLBACK variant_from_pointer(void *ptr);

/* -------------------------------------------------------------------------- */

public variant CALLBACK variant_from_integer(uint64_t i);

/* -------------------------------------------------------------------------- */

public variant CALLBACK variant_from_decimal(double d);

/* -------------------------------------------------------------------------- */

public variant CALLBACK variant_from_boolean(int b);

/* -------------------------------------------------------------------------- */

public variant CALLBACK variant_from_string(m_string *s);

/* -------------------------------------------------------------------------- */

public variant CALLBACK variant_null(void);

/* -------------------------------------------------------------------------- */

public void * CALLBACK variant_to_pointer(variant v);

/* -------------------------------------------------------------------------- */

public uint64_t CALLBACK variant_to_integer(variant v);

/* -------------------------------------------------------------------------- */

public double CALLBACK variant_to_decimal(variant v);

/* -------------------------------------------------------------------------- */

public int CALLBACK variant_to_boolean(variant v);

/* -------------------------------------------------------------------------- */

public m_string * CALLBACK variant_to_string(variant v);

/* -------------------------------------------------------------------------- */

#endif
