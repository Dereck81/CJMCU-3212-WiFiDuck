/*
   Copyright (c) 2019 Stefan Kremser
   This software is licensed under the MIT License. See the license file for details.
   Source: github.com/spacehuhn/SimpleCLI

   Modified and adapted by:
    - Dereck81
 */

#pragma once

#include <stddef.h> // size_t
#include "../../include/config.h"

#define COMPARE_UNEQUAL 0
#define COMPARE_EQUAL 1

#define COMPARE_CASE_INSENSETIVE 0
#define COMPARE_CASE_SENSETIVE 1

#ifdef USE_SD_CARD

#include <stdbool.h>

/* Static pool limits */
#define MAX_LINE_NODES 1  // Only one line at a time. I've tested it with one line and it actually works well, even with very long texts.
#define MAX_WORD_NODES 5  // Sufficient for most commands. Example: CTRL ALT DELETE. If desired, the size can be increased.

/**
 * @brief Compares user input against a command template
 * @return COMPARE_EQUAL or COMPARE_UNEQUAL
 */
int compare(const char* user_str, size_t user_str_len, const char* templ_str, int case_sensetive);

/**
 * @brief Single parsed word (static pool)
 */
typedef struct word_node {
    const char*       str;
    size_t            len;
    struct word_node* next;
} word_node;

/**
 * @brief Linked list of words
 */
typedef struct word_list {
    word_node* first;
    word_node* last;
    size_t     size;
} word_list;

/**
 * @brief Single parsed line
 */
typedef struct line_node {
    const char* str;
    size_t      len;
    word_list   words;
    struct line_node* next;
} line_node;

/**
 * @brief Linked list of lines
 */
typedef struct line_list {
    line_node* first;
    line_node* last;
    size_t     size;
} line_list;

/** @brief Creates a word node from the static pool */
word_node* word_node_create(const char* str, size_t len);

/** @brief Resets internal static pools */
void reset_pools();

/** @brief Creates an empty word list */
word_list* word_list_create();

/** @brief Appends a word to a word list */
void word_list_push(word_list* l, word_node* n);

/** @brief Gets a word by index */
word_node* word_list_get(word_list* l, size_t i);

/** @brief Creates a line node from the static pool */
line_node* line_node_create(const char* str, size_t len);

/** @brief Creates an empty line list */
line_list* line_list_create();

/** @brief Appends a line to a line list */
void line_list_push(line_list* l, line_node* n);

/** @brief Gets a line by index */
line_node* line_list_get(line_list* l, size_t i);

/** @brief Splits a line into words (no heap) */
word_list* parse_words(const char* str, size_t len);

/** @brief Splits a buffer into lines (no heap) */
line_list* parse_lines(const char* str, size_t len);

#else

/**
 * @brief Compares user input against a command template
 * @return COMPARE_EQUAL or COMPARE_UNEQUAL
 */
int compare(const char* user_str, size_t user_str_len, const char* templ_str, int case_sensetive);

/**
 * @brief Single parsed word (heap allocated)
 */
typedef struct word_node {
    const char      * str;
    size_t            len;
    struct word_node* next;
} word_node;

/**
 * @brief Linked list of words
 */
typedef struct word_list {
    struct word_node* first;
    struct word_node* last;
    size_t            size;
} word_list;

/**
 * @brief Single parsed line
 */
typedef struct line_node {
    const char      * str;
    size_t            len;
    struct word_list* words;
    struct line_node* next;
} line_node;

/**
 * @brief Linked list of lines
 */
typedef struct line_list {
    struct line_node* first;
    struct line_node* last;
    size_t            size;
} line_list;

// ===== Word Node ===== //

/** @brief Allocates a word node */
word_node* word_node_create(const char* str, size_t len);

/** @brief Frees a word node */
word_node* word_node_destroy(word_node* n);

/** @brief Recursively frees word nodes */
word_node* word_node_destroy_rec(word_node* n);

// ===== Word List ===== //

/** @brief Creates a word list */
word_list* word_list_create();

/** @brief Destroys a word list */
word_list* word_list_destroy(word_list* l);

/** @brief Appends a word to the list */
void word_list_push(word_list* l, word_node* n);

/** @brief Gets a word by index */
word_node* word_list_get(word_list* l, size_t i);

// ===== Line Node ==== //

/** @brief Allocates a line node */
line_node* line_node_create(const char* str, size_t len);

/** @brief Frees a line node */
word_node* line_node_destroy(line_node* n);

/** @brief Recursively frees line nodes */
word_node* line_node_destroy_rec(line_node* n);

// ===== Line List ===== //

/** @brief Creates a line list */
line_list* line_list_create();

/** @brief Destroys a line list */
line_list* line_list_destroy(line_list* l);

/** @brief Appends a line to the list */
void line_list_push(line_list* l, line_node* n);

/** @brief Gets a line by index */
line_node* line_list_get(line_list* l, size_t i);

// ===== Parser ===== //

/** @brief Splits a line into words (heap allocated) */
word_list* parse_words(const char* str, size_t len);

/** @brief Splits a buffer into lines (heap allocated) */
line_list* parse_lines(const char* str, size_t len);

#endif