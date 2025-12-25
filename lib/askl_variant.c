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

#include "askl_variant.h"

/* -------------------------------------------------------------------------- */

public variant CALLBACK variant_from_pointer(void *ptr)
{
    variant v = { 0 };
    v.metadata.fields.type = VALUE_POINTER;
    v.value.pointer = ptr;
    return v;
}

/* -------------------------------------------------------------------------- */

public variant CALLBACK variant_from_integer(uint64_t i)
{
    variant v = { 0 };
    v.metadata.fields.type = VALUE_INTEGER;
    v.value.integer = i;
    return v;
}

/* -------------------------------------------------------------------------- */

public variant CALLBACK variant_from_decimal(double d)
{
    variant v = { 0 };
    v.metadata.fields.type = VALUE_DECIMAL;
    v.value.decimal = d;
    return v;
}

/* -------------------------------------------------------------------------- */

public variant CALLBACK variant_from_boolean(int b)
{
    variant v = { 0 };
    v.metadata.fields.type = VALUE_BOOLEAN;
    v.value.integer = !! b;
    return v;
}

/* -------------------------------------------------------------------------- */

public variant CALLBACK variant_from_string(m_string *s)
{
    variant v = { 0 };
    v.metadata.fields.type = VALUE_STRING;
    v.value.pointer = s;
    return v;
}

/* -------------------------------------------------------------------------- */

public variant CALLBACK variant_null(void)
{
    variant v = { 0 };
    v.metadata.fields.type = VALUE_NULL;
    v.value.pointer = NULL;
    return v;
}

/* -------------------------------------------------------------------------- */

public void * CALLBACK variant_to_pointer(variant v)
{
    if (! is_pointer(v)) {
        /* tolerate VALUE_NULL and _VALUE_OBJECT */
        if (! _is_object(v) && v.metadata.fields.type != VALUE_NULL)
            die("type error");
    }
    return v.value.pointer;
}

/* -------------------------------------------------------------------------- */

public uint64_t CALLBACK variant_to_integer(variant v)
{
    if (! is_integer(v)) die("type error");

    return v.value.integer;
}

/* -------------------------------------------------------------------------- */

public double CALLBACK variant_to_decimal(variant v)
{
    if (! is_decimal(v)) die("type error");
    return v.value.decimal;
}

/* -------------------------------------------------------------------------- */

public int CALLBACK variant_to_boolean(variant v)
{
    if (! is_boolean(v)) die("type error");
    return (v.value.integer);
}

/* -------------------------------------------------------------------------- */

public m_string * CALLBACK variant_to_string(variant v)
{
    if (! is_string(v)) die("type error");
    return v.value.pointer;
}

/* -------------------------------------------------------------------------- */
