/*
 * Copyright (c) 2020 Lucas Müller
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include <libjscon.h>
#include "jscon-common.h"

#include "strscpy.h"
#include "debug.h"


/* reentrant hashtable linking function */
void
Jscon_composite_link_r(jscon_item_t *item, jscon_composite_t **p_last_accessed_comp)
{
    DEBUG_ASSERT(IS_COMPOSITE(item), "Item is not an Object or Array");

    jscon_composite_t *last_accessed_comp = *p_last_accessed_comp;
    if (NULL != last_accessed_comp){
        last_accessed_comp->next = item->comp; //item is not root
        item->comp->prev = last_accessed_comp;
    }

    last_accessed_comp = item->comp;

    *p_last_accessed_comp = last_accessed_comp;
}

void
Jscon_composite_build(jscon_item_t *item)
{
    DEBUG_ASSERT(IS_COMPOSITE(item), "Item is not an Object or Array");

    hashtable_build(item->comp->hashtable, 2 + (1.3 * item->comp->num_branch)); //30% size increase to account for future expansions, and a default bucket size of 2

    item->comp->p_item = item;

    for (size_t i=0; i < item->comp->num_branch; ++i){
        Jscon_composite_set(item->comp->branch[i]->key, item->comp->branch[i]);
    }
}

jscon_item_t*
Jscon_composite_get(const char *key, jscon_item_t *item)
{
    if (!IS_COMPOSITE(item)) return NULL;

    jscon_composite_t *comp = item->comp;
    return hashtable_get(comp->hashtable, key);
}

jscon_item_t*
Jscon_composite_set(const char *key, jscon_item_t *item)
{
    DEBUG_ASSERT(!IS_ROOT(item), "Can't add to parent hashtable if Item is root");

    jscon_composite_t *comp = item->parent->comp;
    return hashtable_set(comp->hashtable, key, item);
}

/* remake hashtable on functions that deal with increasing branches */
void
Jscon_composite_remake(jscon_item_t *item)
{
    hashtable_destroy(item->comp->hashtable);

    item->comp->hashtable = hashtable_init();
    DEBUG_ASSERT(NULL != item->comp->hashtable, "Out of memory");

    Jscon_composite_build(item);
}

jscon_composite_t*
Jscon_decode_composite(char **p_buffer, size_t n_branch){
    jscon_composite_t *new_comp = calloc(1, sizeof *new_comp);
    DEBUG_ASSERT(NULL != new_comp, "Out of memory");

    new_comp->hashtable = hashtable_init(); 
    DEBUG_ASSERT(NULL != new_comp->hashtable, "Out of memory");

    new_comp->branch = malloc((1+n_branch) * sizeof(jscon_item_t*));
    DEBUG_ASSERT(NULL != new_comp->branch, "Out of memory");

    ++*p_buffer; //skips composite's '{' or '[' delim

    return new_comp;
}

char*
Jscon_decode_string(char **p_buffer)
{
    char *start = *p_buffer;
    DEBUG_ASSERT('\"' == *start, "Not a string"); //makes sure a string is given

    char *end = ++start;
    while (('\0' != *end) && ('\"' != *end)){
        if ('\\' == *end++){ //skips escaped characters
            ++end;
        }
    }
    DEBUG_ASSERT('\"' == *end, "Not a string"); //makes sure end of string exists

    *p_buffer = end + 1; //skips double quotes buffer position

    char *set_str = strndup(start, end-start);
    DEBUG_ASSERT(NULL != set_str, "Out of memory");

    return set_str;
}

double
Jscon_decode_double(char **p_buffer)
{
    char *start = *p_buffer;
    char *end = start;

    /* 1st STEP: check for a minus sign and skip it */
    if ('-' == *end){
        ++end; //skips minus sign
    }

    /* 2nd STEP: skips until a non digit char found */
    DEBUG_ASSERT(isdigit(*end), "Not a number"); //interrupt if char isn't digit
    while (isdigit(*++end))
        continue; //skips while char is digit

    /* 3rd STEP: if non-digit char is not a comma then it must be
        an integer*/
    if ('.' == *end){
        while (isdigit(*++end))
            continue;
    }

    /* 4th STEP: if exponent found skips its tokens */
    if (('e' == *end) || ('E' == *end)){
        ++end;
        if (('+' == *end) || ('-' == *end)){ 
            ++end;
        }
        DEBUG_ASSERT(isdigit(*end), "Not a number");
        while (isdigit(*++end))
            continue;
    }

    /* 5th STEP: convert string to double and return its value */
    char numstr[MAX_DIGITS];
    strscpy(numstr, start, ((size_t)(end-start+1) < sizeof(numstr)) ? (size_t)(end-start+1) : sizeof(numstr));

    double set_double;
    sscanf(numstr,"%lf",&set_double);

    *p_buffer = end; //skips entire length of number

    return set_double;
}

bool
Jscon_decode_boolean(char **p_buffer)
{
    if ('t' == **p_buffer){
        *p_buffer += 4; //skips length of "true"
        return true;
    }
    *p_buffer += 5; //skips length of "false"
    return false;
}

void
Jscon_decode_null(char **p_buffer){
    *p_buffer += 4; //skips length of "null"
}
