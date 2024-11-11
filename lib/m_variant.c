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

#include "m_variant.h"

/* -------------------------------------------------------------------------- */

public variant CALLBACK value_from_pointer(void *ptr)
{
    variant v;
    v.type = VALUE_POINTER;
    v.value.pointer = ptr;
    return v;
}

/* -------------------------------------------------------------------------- */

public variant CALLBACK value_from_integer(uint64_t i)
{
    variant v;
    v.type = VALUE_INTEGER;
    v.value.integer = i;
    return v;
}

/* -------------------------------------------------------------------------- */

public variant CALLBACK value_from_decimal(double d)
{
    variant v;
    v.type = VALUE_DECIMAL;
    v.value.decimal = d;
    return v;
}

/* -------------------------------------------------------------------------- */

public variant CALLBACK value_from_boolean(int b)
{
    variant v;
    v.type = VALUE_BOOL;
    if (b) v.type |= VALUE_TRUE;
    v.value.integer = 0;
    return v;
}

/* -------------------------------------------------------------------------- */

public variant CALLBACK value_from_string(m_string *s)
{
    variant v;
    v.type = (VALUE_POINTER | _VALUE_STRING);
    v.value.pointer = s;
    return v;
}

/* -------------------------------------------------------------------------- */

public variant CALLBACK value_null(void)
{
    variant v;
    v.type = (VALUE_POINTER | VALUE_NULL);
    v.value.pointer = NULL;
    return v;
}

/* -------------------------------------------------------------------------- */

public void * CALLBACK value_to_pointer(variant v)
{
    if (~v.type & VALUE_POINTER)
        die("type error");

    return v.value.pointer;
}

/* -------------------------------------------------------------------------- */

public uint64_t CALLBACK value_to_integer(variant v)
{
    if (v.type != VALUE_INTEGER)
        die("type error");

    return v.value.integer;
}

/* -------------------------------------------------------------------------- */

public double CALLBACK value_to_decimal(variant v)
{
    if (v.type != VALUE_DECIMAL)
        die("type error");

    return v.value.decimal;
}

/* -------------------------------------------------------------------------- */

public int CALLBACK value_to_boolean(variant v)
{
    if (~v.type & VALUE_BOOL)
        die("type error");

    return (v.type & VALUE_TRUE);
}

/* -------------------------------------------------------------------------- */

public m_string * CALLBACK value_to_string(variant v)
{
    if (v.type != (VALUE_POINTER | _VALUE_STRING))
        die("type error");

    return v.value.pointer;
}

/* -------------------------------------------------------------------------- */
