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

#include <assert.h>
#include <stdio.h>
#include <unistd.h> //for access()
#include <string.h>
#include <locale.h>

#include <libjscon.h>

/* forwarding */
FILE *select_output(int argc, char *argv[]);
char *get_buffer(char filename[]);
jscon_item_st *callback_test(jscon_item_st *item);

int main(int argc, char *argv[])
{
  char *locale = setlocale(LC_CTYPE, "");
  assert(locale);

  FILE *f_out = select_output(argc, argv);
  char *buffer = get_buffer(argv[1]);

  jscon_item_st *item[3] = {NULL};
  int integer1, integer2;
  char str1[25] = {0};
  double double1 = 0.0;

  //jscon_scanf(buffer, "#meta%ji,#data%ji,#string%ji,#a%jd,#b%js,#c%jf", &item[0], &item[1], &item[2], &integer1, str1, &double1);
  jscon_scanf(buffer, 
              "#t%js " \
              "#s%jd " \
              "#op%jd " \
              "#d%ji",
               str1,
               &integer1,
               &integer2,
               &item[0]);
  
  for (size_t i=0; i<3; ++i){
    if (NULL == item[i])
    continue;

    char *buffer = jscon_stringify(item[i], JSCON_ANY);
    //fprintf(stdout, "%s: %s\n", jscon_get_key(item[i]), buffer);
    free(buffer);
    jscon_destroy(item[i]);
  }

  fprintf(stdout, "integer1: %d\n", integer1);
  fprintf(stdout, "integer2: %d\n", integer2);
  fprintf(stdout, "str1: %s\n", str1);
  fprintf(stdout, "double1: %f\n", double1);

  //jscon_parser_callback(&callback_test);
  jscon_item_st *root = jscon_parse(buffer);

  jscon_item_st *property1 = jscon_dettach(jscon_get_branch(root, "author"));
  if (NULL != property1){
    char *buffer = jscon_stringify(property1, JSCON_ANY);
    fprintf(stdout, "%s: %s\n", jscon_get_key(property1), buffer);
    free(buffer);
    jscon_destroy(property1);
  }

  fprintf(stdout, "key: meta, index: %ld\n", jscon_get_index(root, "meta"));
  fprintf(stdout, "key: data, index: %ld\n", jscon_get_index(root, "data"));
  fprintf(stdout, "key: string, index: %ld\n", jscon_get_index(root, "string"));

  jscon_item_st *tmp, *current_item = NULL;
  char *test1_buffer;
  jscon_item_st *walk1 = jscon_iter_composite_r(root, &current_item);
  do {
    tmp = jscon_get_branch(walk1, "m");
    if (NULL != tmp){
      test1_buffer = jscon_stringify(tmp, JSCON_ANY);
      fwrite(test1_buffer, 1, strlen(test1_buffer), stderr);
      fputc('\n', stderr);
      free(test1_buffer);
    }

    walk1 = jscon_iter_composite_r(NULL, &current_item);
  } while (NULL != walk1);

  jscon_item_st *walk2 = root;
  for (int i=0; i < 5 && walk2; ++i){
    fprintf(stderr, "%s\n", walk2->key);
    walk2 = jscon_iter_next(walk2);
  }

  walk2 = root;
  do {
    fprintf(stderr, "%s\n", walk2->key);
    walk2 = jscon_iter_next(walk2);
  } while (NULL != walk2);

  char *test2_buffer = jscon_stringify(root, JSCON_ANY);
  fwrite(test2_buffer, 1, strlen(test2_buffer), f_out);
  free(test2_buffer);
  jscon_destroy(root);

  free(buffer);
  fclose(f_out);

  return EXIT_SUCCESS;
}

FILE *select_output(int argc, char *argv[])
{
  char *p_arg=NULL;
  while (argc--){
    p_arg = *argv++;
    if ((*p_arg++ == '-') && (*p_arg++ == 'o') && (*p_arg == '\0')){
      assert (argc == 1); //check if theres exactly one arg left

      char *file = *argv;
      assert(access(file, W_OK)); //check if file exists

      return fopen(file, "w");
    }
  }

  return fopen("data.txt", "w");
}

/* returns file size in long format */
static long
fetch_filesize(FILE *p_file)
{
  fseek(p_file, 0, SEEK_END);
  long filesize = ftell(p_file);
  assert(filesize > 0);
  fseek(p_file, 0, SEEK_SET);

  return filesize;
}

/* returns file content */
static char*
read_file(FILE* p_file, long filesize)
{
  char *buffer = malloc(filesize+1);
  assert(NULL != buffer);

  //read file into buffer
  fread(buffer,1,filesize,p_file);
  buffer[filesize] = 0;

  return buffer;
}

/* returns buffer containing file content */
char*
get_buffer(char filename[])
{
  FILE *file = fopen(filename, "rb");
  assert(NULL != file);

  long filesize = fetch_filesize(file);
  char *buffer = read_file(file, filesize);

  fclose(file);

  return buffer;
}

jscon_item_st *callback_test(jscon_item_st *item)
{
  if (NULL != item && jscon_keycmp(item, "m")){
    fprintf(stdout, "%s\n", jscon_get_string(item));
  }
    
  return item;
}
