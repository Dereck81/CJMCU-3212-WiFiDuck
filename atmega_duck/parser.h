/*
   Copyright (c) 2019 Stefan Kremser
   This software is licensed under the MIT License. See the license file for details.
   Source: github.com/spacehuhn/SimpleCLI
 */

#pragma once

#include <stddef.h> // size_t
#include "config.h"

#define COMPARE_UNEQUAL 0
#define COMPARE_EQUAL 1

#define COMPARE_CASE_INSENSETIVE 0
#define COMPARE_CASE_SENSETIVE 1

#ifdef USE_SD_CARD

#include <stdbool.h>

/* This code is useful when RAM is low. In this case, using the SD card increases RAM usage, 
   and using the original code with malloc often fails, leaving the ATmega32u4 in a zombie state. 
   However, when there is plenty of RAM, the project owner's code works perfectly.
*/
#define MAX_LINE_NODES 1  // Only one line at a time. I've tested it with one line and it actually works well, even with very long texts.
#define MAX_WORD_NODES 5  // Sufficient for most commands. Example: CTRL ALT DELETE. If desired, the size can be increased.

int compare(const char* user_str, size_t user_str_len, const char* templ_str, int case_sensetive);

typedef struct word_node {
    const char*       str;
    size_t            len;
    struct word_node* next;
} word_node;

typedef struct word_list {
    word_node* first;
    word_node* last;
    size_t     size;
} word_list;

typedef struct line_node {
    const char* str;
    size_t      len;
    word_list   words;
    struct line_node* next;
} line_node;

typedef struct line_list {
    line_node* first;
    line_node* last;
    size_t     size;
} line_list;

word_node* word_node_create(const char* str, size_t len);
void reset_pools();

word_list* word_list_create();
void word_list_push(word_list* l, word_node* n);
word_node* word_list_get(word_list* l, size_t i);

line_node* line_node_create(const char* str, size_t len);
line_list* line_list_create();
void line_list_push(line_list* l, line_node* n);
line_node* line_list_get(line_list* l, size_t i);

word_list* parse_words(const char* str, size_t len);
line_list* parse_lines(const char* str, size_t len);

#else

int compare(const char* user_str, size_t user_str_len, const char* templ_str, int case_sensetive);

typedef struct word_node {
    const char      * str;
    size_t            len;
    struct word_node* next;
} word_node;

typedef struct word_list {
    struct word_node* first;
    struct word_node* last;
    size_t            size;
} word_list;

typedef struct line_node {
    const char      * str;
    size_t            len;
    struct word_list* words;
    struct line_node* next;
} line_node;

typedef struct line_list {
    struct line_node* first;
    struct line_node* last;
    size_t            size;
} line_list;

// ===== Word Node ===== //
word_node* word_node_create(const char* str, size_t len);
word_node* word_node_destroy(word_node* n);
word_node* word_node_destroy_rec(word_node* n);

// ===== Word List ===== //
word_list* word_list_create();
word_list* word_list_destroy(word_list* l);

void word_list_push(word_list* l, word_node* n);
word_node* word_list_get(word_list* l, size_t i);

// ===== Line Node ==== //
line_node* line_node_create(const char* str, size_t len);
word_node* line_node_destroy(line_node* n);
word_node* line_node_destroy_rec(line_node* n);

// ===== Line List ===== //
line_list* line_list_create();
line_list* line_list_destroy(line_list* l);

void line_list_push(line_list* l, line_node* n);
line_node* line_list_get(line_list* l, size_t i);

// ===== Parser ===== //
word_list* parse_words(const char* str, size_t len);
line_list* parse_lines(const char* str, size_t len);

#endif