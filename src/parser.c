#include "../JSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

/* create new json item and return it's address */
static JsonItem*
JsonItem_PropertyCreate(JsonItem *item)
{
  ++item->n_property; //update object's property count
  //update memory space for property's list
  item->property = realloc(item->property, item->n_property*sizeof(JsonItem*));
  assert(item->property);
  //allocate memory space for new property (nested item)
  item->property[item->n_property-1] = calloc(1,sizeof(JsonItem));
  assert(item->property[item->n_property-1]);
  //get parent address of the new property
  item->property[item->n_property-1]->parent = item;
  //return new property address
  return item->property[item->n_property-1];
}

/* Destroy current item and all of its nested object/arrays */
static void
JsonItem_Destroy(JsonItem *item)
{
  for (size_t i=0; i < item->n_property; ++i){
    JsonItem_Destroy(item->property[i]);
  }
  free(item->property);

  if (item->dtype == String){
    free(item->string);
  }
  free(item);
}

Json*
Json_Create()
{
  Json *new_json = calloc(1,sizeof(Json));
  assert(new_json);

  new_json->root = calloc(1,sizeof(JsonItem));
  assert(new_json->root);

  return new_json;
}

/* Destroy json struct */
void
Json_Destroy(Json *json)
{
  JsonItem_Destroy(json->root);
  
  while (json->n_list){
    free(*json->list_ptr_key[--json->n_list]);
    free(json->list_ptr_key[json->n_list]);
  }
  free(json->list_ptr_key);
  free(json);
}

static JsonString*
JsonString_CacheKey(Json *json, const JsonString cache_entry)
{
  ++json->n_list;

  json->list_ptr_key = realloc(json->list_ptr_key, json->n_list*sizeof(char**));
  assert(json->list_ptr_key);

  int i = json->n_list-1;
  while ((i > 0) && (strcmp(cache_entry, *json->list_ptr_key[i-1]) < 0)){
    json->list_ptr_key[i] = json->list_ptr_key[i-1];
    --i;
  }
  //this extra space will be necessary for
  // doing replace operations
  JsonString *ptr_new_key = malloc(sizeof(JsonString));
  assert(ptr_new_key);
  *ptr_new_key = strndup(cache_entry,strlen(cache_entry));
  assert(*ptr_new_key);

  json->list_ptr_key[i] = ptr_new_key;
  return json->list_ptr_key[i];
}

static JsonString*
JsonString_GetKey(Json *json, const JsonString cache_entry)
{
  int found_index = Json_SearchKey(json, cache_entry);
  if (found_index != -1)
    return json->list_ptr_key[found_index];
  // if key not found, create it and save in cache
  return JsonString_CacheKey(json, cache_entry);
}

/* get numerical key for array type
    json data, in string format */
static JsonString* //fix: get asprintf to work
JsonString_SetArrKey(Json *json, JsonItem *item)
{
  char cache_entry[MAX_DIGITS];
  snprintf(cache_entry,MAX_DIGITS-1,"%ld",item->n_property);

  return JsonString_GetKey(json, cache_entry);
}

static JsonString
JsonString_Set(char **ptr_buffer)
{
  char *start = *ptr_buffer;
  assert(*start == '\"');

  char *end = ++start;
  while ((*end) && (*end != '\"')){
    if (*end++ == '\\'){ //skips \" char
      ++end;
    }
  }
  assert(*end == '\"');
  *ptr_buffer = end+1; //store position after double quotes

  JsonString set_str = strndup(start, end-start);
  assert(set_str);

  return set_str;
}

static void
JsonString_SetStatic(char **ptr_buffer, JsonString set_str, const int n)
{
  char *start = *ptr_buffer;
  assert(*start == '\"');

  char *end = ++start;
  while ((*end) && (*end != '\"')){
    if (*end++ == '\\'){ //skips \" char
      ++end;
    }
  }
  assert(*end == '\"');
  *ptr_buffer = end+1; //store position after double quotes
  
  if (n < end-start)
    strncpy(set_str, start, n);
  else
    strncpy(set_str, start, end-start);
}

static JsonNumber
JsonNumber_Set(char **ptr_buffer)
{
  char *start = *ptr_buffer;
  char *end = start;

  if (*end == '-'){
    ++end;
  }

  assert(isdigit(*end));
  while (isdigit(*++end))
    continue;

  if (*end == '.'){
    while (isdigit(*++end))
      continue;
  }
  //check for exponent
  if ((*end == 'e') || (*end == 'E')){
    ++end;
    if ((*end == '+') || (*end == '-')){
      ++end;
    }
    assert(isdigit(*end));
    while (isdigit(*++end))
      continue;
  }

  assert(end-start <= MAX_DIGITS);
  char get_numstr[MAX_DIGITS] = {0};
  strncpy(get_numstr, start, end-start);

  JsonNumber set_number;
  sscanf(get_numstr,"%lf",&set_number);

  *ptr_buffer = end;

  return set_number;
}

/* get and return data from appointed datatype */
static JsonItem*
JsonItem_SetValue(JsonDType get_dtype, JsonItem *item, char **ptr_buffer)
{
  item->dtype = get_dtype;

  switch (item->dtype){
    case String:
      item->string = JsonString_Set(ptr_buffer);
      break;
    case Number:
      item->number = JsonNumber_Set(ptr_buffer);
      break;
    case Boolean:
      if (**ptr_buffer == 't'){
        *ptr_buffer += 4; //length of "true"
        item->boolean = 1;
        break;
      }
      *ptr_buffer += 5; //length of "false"
      item->boolean = 0;
      break;
    case Null:
      *ptr_buffer += 4; //length of "null"
      break;
    case Array:
    case Object:
      ++*ptr_buffer;
      break;
    default:
      fprintf(stderr,"ERROR: invalid datatype %ld\n", item->dtype);
      exit(EXIT_FAILURE);
  }

  return item;
}

/* create nested object and return
    the nested object address. */
static JsonItem*
JsonItem_SetIncomplete(JsonItem *item, JsonString *get_ptr_key, JsonDType get_dtype, char **ptr_buffer)
{
  item = JsonItem_PropertyCreate(item);
  item->ptr_key = get_ptr_key;
  item = JsonItem_SetValue(get_dtype, item, ptr_buffer);

  return item;
}

static JsonItem*
JsonItem_Wrap(JsonItem *item){
  return item->parent; //wraps property in item (completes it)
}

/* create property from appointed JSON datatype
    and return the item containing it */
static JsonItem*
JsonItem_SetComplete(JsonItem *item, JsonString *get_ptr_key, JsonDType get_dtype, char **ptr_buffer)
{
  item = JsonItem_SetIncomplete(item, get_ptr_key, get_dtype, ptr_buffer);
  return JsonItem_Wrap(item);
}

static JsonItem*
JsonItem_BuildArray(Json *json, JsonItem *item, char **ptr_buffer)
{
  JsonItem* (*item_setter)(JsonItem*,JsonString*,JsonDType,char**);
  JsonDType set_dtype;

  switch (**ptr_buffer){
    case ']':/*ARRAY WRAPPER DETECTED*/
      ++*ptr_buffer;
      return JsonItem_Wrap(item);
    case '{':/*OBJECT DETECTED*/
      set_dtype = Object;
      item_setter = &JsonItem_SetIncomplete;
      break;
    case '[':/*ARRAY DETECTED*/
      set_dtype = Array;
      item_setter = &JsonItem_SetIncomplete;
      break;
    case '\"':/*STRING DETECTED*/
      set_dtype = String;
      item_setter = &JsonItem_SetComplete;
      break;
    case 't':/*CHECK FOR*/
    case 'f':/* BOOLEAN */
      if (strncmp(*ptr_buffer,"true",4) 
          && strncmp(*ptr_buffer,"false",5)){
        goto error;
      }
      set_dtype = Boolean;
      item_setter = &JsonItem_SetComplete;
      break;
    case 'n':/*CHECK FOR NULL*/
      if (strncmp(*ptr_buffer,"null",4)){
        goto error;
      }
      set_dtype = Null;
      item_setter = &JsonItem_SetComplete;
      break;
    case ',': /*ignore comma*/
      ++*ptr_buffer;
      return item;
    default:
      /*IGNORE CONTROL CHARACTER*/
      if (isspace(**ptr_buffer)
          || iscntrl(**ptr_buffer)){
        ++*ptr_buffer;
        return item;
      }
      /*CHECK FOR NUMBER*/
      if (!isdigit(**ptr_buffer)
          && (**ptr_buffer != '-')){
        goto error;
      }
      set_dtype = Number;
      item_setter = &JsonItem_SetComplete;
      break;
  }

  return (*item_setter)(item,JsonString_SetArrKey(json,item),set_dtype,ptr_buffer);

  error:
    fprintf(stderr,"ERROR: invalid json token %c\n", **ptr_buffer);
    exit(EXIT_FAILURE);
}

static JsonItem*
JsonItem_BuildObject(Json *json, JsonItem *item, JsonString **ptr_key, char **ptr_buffer)
{
  JsonItem* (*item_setter)(JsonItem*,JsonString*,JsonDType,char**);
  JsonDType set_dtype;
  char set_key[KEY_LENGTH] = {0};

  switch (**ptr_buffer){
    case '}':/*OBJECT WRAPPER DETECTED*/
      ++*ptr_buffer;
      return JsonItem_Wrap(item);
    case '\"':/*KEY STRING DETECTED*/
      JsonString_SetStatic(ptr_buffer,set_key,KEY_LENGTH);
      *ptr_key = JsonString_GetKey(json,set_key);
      return item;
    case ':':/*VALUE DETECTED*/
        do { //skips space and control characters before next switch
          ++*ptr_buffer;
        } while (isspace(**ptr_buffer) || iscntrl(**ptr_buffer));

        switch (**ptr_buffer){ //fix: move to function
          case '{':/*OBJECT DETECTED*/
            set_dtype = Object;
            item_setter = &JsonItem_SetIncomplete;
            break;
          case '[':/*ARRAY DETECTED*/
            set_dtype = Array;
            item_setter = &JsonItem_SetIncomplete;
            break;
          case '\"':/*STRING DETECTED*/
            set_dtype = String;
            item_setter = &JsonItem_SetComplete;
            break;
          case 't':/*CHECK FOR*/
          case 'f':/* BOOLEAN */
            if (strncmp(*ptr_buffer,"true",4)
                && strncmp(*ptr_buffer,"false",5)){
              goto error;
            }
            set_dtype = Boolean;
            item_setter = &JsonItem_SetComplete;
            break;
          case 'n':/*CHECK FOR NULL*/
            if (strncmp(*ptr_buffer,"null",4)){
              goto error; 
            }
            set_dtype = Null;
            item_setter = &JsonItem_SetComplete;
            break;
          default:
            /*CHECK FOR NUMBER*/
            if (!isdigit(**ptr_buffer)
                && (**ptr_buffer != '-')){
              goto error;
            }
            set_dtype = Number;
            item_setter = &JsonItem_SetComplete;
            break;
        }
      return (*item_setter)(item,*ptr_key,set_dtype,ptr_buffer);
    case ',': //ignore comma
      ++*ptr_buffer;
      return item;
    default:
      /*IGNORE CONTROL CHARACTER*/
      if (isspace(**ptr_buffer) || iscntrl(**ptr_buffer)){
        ++*ptr_buffer;
        return item;
      }
      goto error;
  }

  error:
    fprintf(stderr,"ERROR: invalid json token %c\n", **ptr_buffer);
    exit(EXIT_FAILURE);
}

static JsonItem*
JsonItem_BuildEntity(JsonItem *item, char **ptr_buffer)
{
  JsonDType set_dtype;

  switch (**ptr_buffer){
    case '{':/*OBJECT DETECTED*/
      set_dtype = Object;
      break;
    case '[':/*ARRAY DETECTED*/
      set_dtype = Array;
      break;
    case '\"':/*STRING DETECTED*/
      set_dtype = String;
      break;
    case 't':/*CHECK FOR*/
    case 'f':/* BOOLEAN */
      if (strncmp(*ptr_buffer,"true",4)
          && strncmp(*ptr_buffer,"false",5)){
        goto error;
      }
      set_dtype = Boolean;
      break;
    case 'n':/*CHECK FOR NULL*/
      if (strncmp(*ptr_buffer,"null",4)){
        goto error;
      }
      set_dtype = Null;
      break;
    default:
      /*IGNORE CONTROL CHARACTER*/
      if (isspace(**ptr_buffer)
          || iscntrl(**ptr_buffer)){
        ++*ptr_buffer;
        return item;
      }
      /*CHECK FOR NUMBER*/
      if (!isdigit(**ptr_buffer)
          && (**ptr_buffer != '-')){
        goto error;
      }
      set_dtype = Number;
      break;
  }

  return JsonItem_SetValue(set_dtype, item, ptr_buffer);

  error:
    fprintf(stderr,"ERROR: invalid json token %c\n",**ptr_buffer);
    exit(EXIT_FAILURE);
}

/* get json  by evaluating buffer's current position */
static JsonItem*
JsonItem_Build(Json *json, JsonItem*item, JsonString **ptr_key, char **ptr_buffer)
{
  switch(item->dtype){
    case Object:
      return JsonItem_BuildObject(json,item,ptr_key,ptr_buffer);
    case Array:
      return JsonItem_BuildArray(json,item,ptr_buffer);
    case Undefined://this should be true only at the first call
      return JsonItem_BuildEntity(item,ptr_buffer);
    default: //nothing else to build, check buffer for potential error
      if (isspace(**ptr_buffer) || iscntrl(**ptr_buffer)){
        ++*ptr_buffer; //moves if cntrl character found ('\n','\b',..)
        return item;
      }
      goto error;
  }

  error:
    fprintf(stderr,"ERROR: invalid json token %c\n",**ptr_buffer);
    exit(EXIT_FAILURE);
}

Json*
Json_Parse(char *buffer)
{
  Json *json = Json_Create();

  JsonString *ptr_set_key;
  JsonItem *item = json->root;
  //build while item and buffer aren't nulled
  while (item && *buffer){
    item = JsonItem_Build(json,item,&ptr_set_key,&buffer);
  }

  return json;
}

static void
apply_reviver(JsonItem *item, void (*reviver)(JsonItem*))
{
  (*reviver)(item);
  for (size_t i=0; i < item->n_property; ++i){
    apply_reviver(item->property[i], reviver);
  }
}

Json*
Json_ParseReviver(char *buffer, void (*reviver)(JsonItem*))
{
  Json *json = Json_Parse(buffer);

  if (reviver){
    apply_reviver(json->root, reviver);
  }

  return json;
}

