/*
   Copyright (c) 2019 Stefan Kremser
   This software is licensed under the MIT License. See the license file for details.
   Source: github.com/spacehuhn/SimpleCLI

   Modified and adapted by:
    - Dereck81
 */

#include "parser.h"

#include <string.h>  // strlen
#include <stdbool.h> // bool

/* * NOTE: To prevent the ATmega32u4 from entering a "zombie state", 
 * avoid exceeding 85% RAM usage. Static pooling is used here 
 * to ensure memory stability when using the SD card.
 */

#ifdef USE_SD_CARD

#include <stdint.h>

/**
 * @brief Static memory pools for parser nodes
 *
 * These pools replace dynamic allocation when USE_SD_CARD is enabled.
 * They prevent heap fragmentation and ensure deterministic memory usage
 * during long-running SD card script execution.
 *
 * Pools are reset before each parse cycle.
 */
static line_node linePool[MAX_LINE_NODES];
static word_node wordPool[MAX_WORD_NODES];
static uint8_t nextLine = 0;

/**
 * @brief Resets the static memory pools used by the parser
 *
 * This function resets the internal indices of the static line and word pools.
 * It must be called before starting a new parse cycle to ensure that previously
 * allocated nodes are reused safely.
 *
 * This function is only available when USE_SD_CARD is defined.
 */
void reset_pools() {
    nextLine = 0;
}

/**
 * @brief Converts an ASCII character to lowercase
 *
 * This is a lightweight replacement for tolower() to avoid pulling in
 * locale-dependent or heavy implementations on constrained systems.
 *
 * Only ASCII A–Z characters are converted. All other characters are returned
 * unchanged.
 *
 * @param c Input character
 * @return Lowercase character if applicable, otherwise the original character
 */

char to_lower(char c) {
    return (c >= 65 && c <= 90) ? c + 32 : c;
}

/**
 * @brief Compares a user-provided string against a command template
 *
 * This function checks whether a user input string matches a template string.
 *
 * Case-sensitive and case-insensitive comparisons are supported.
 *
 * @param user_str        Pointer to the user input string
 * @param user_str_len    Length of the user input string
 * @param templ_str       Command template string
 * @param case_sensitive Comparison mode
 *
 * @return COMPARE_EQUAL if the strings match, otherwise COMPARE_UNEQUAL
 */

int compare(const char* user_str, size_t user_str_len, const char* templ_str, int case_sensitive) {
    if (!user_str || !templ_str) return COMPARE_UNEQUAL;

    size_t key_len = strlen(templ_str);

    if (user_str_len != key_len) return COMPARE_UNEQUAL;

    bool match;
    
    if (case_sensitive == COMPARE_CASE_SENSETIVE) match = (memcmp(user_str, templ_str, key_len) == 0);
    else match = (strncasecmp(user_str, templ_str, key_len) == 0);

    return match ? COMPARE_EQUAL : COMPARE_UNEQUAL;
}

/**
 * @brief Creates a word node from the static word pool
 *
 * This function allocates a word node from a fixed-size static pool.
 * No dynamic memory allocation is performed.
 *
 * If the pool is exhausted, NULL is returned and parsing stops gracefully.
 *
 * @param str Pointer to the start of the word in the source buffer
 * @param len Length of the word
 * @return Pointer to a word_node, or NULL if the pool is full
 */
word_node* word_node_create(const char* str, size_t len) {
    // nextWord is handled locally in parse_words()
    static uint8_t nextWord = 0;
    
    if (nextWord >= MAX_WORD_NODES) return NULL;
    
    word_node* n = &wordPool[nextWord++];
    n->str = str;
    n->len = len;
    n->next = NULL;
    return n;
}

/**
 * @brief Creates a line node from the static line pool
 *
 * Initializes an empty word list for the line and links it into the
 * line list during parsing.
 *
 * @param str Pointer to the start of the line in the source buffer
 * @param len Length of the line
 * @return Pointer to a line_node, or NULL if the pool is full
 */
line_node* line_node_create(const char* str, size_t len) {
    if (nextLine >= MAX_LINE_NODES) return NULL;
    
    line_node* n = &linePool[nextLine++];
    n->str = str;
    n->len = len;
    n->words.first = NULL;
    n->words.last = NULL;
    n->words.size = 0;
    n->next = NULL;
    return n;
}

/**
 * @brief Initializes a word list structure
 *
 * Returns a pointer to a statically allocated word_list structure.
 * The list itself does not own any memory and only links existing nodes.
 *
 * @return Pointer to an initialized word_list
 */
word_list* word_list_create() {
    static word_list wl;
    wl.first = NULL;
    wl.last = NULL;
    wl.size = 0;
    return &wl;
}

/**
 * @brief Initializes a line list structure
 *
 * Returns a pointer to a statically allocated line_list structure.
 * Nodes are linked in FIFO order.
 *
 * @return Pointer to an initialized line_list
 */
line_list* line_list_create() {
    static line_list ll;
    ll.first = NULL;
    ll.last = NULL;
    ll.size = 0;
    return &ll;
}

/**
 * @brief Appends a word node to a word list
 *
 * The node is appended at the end of the list.
 *
 * @param l Target word list
 * @param n Word node to append
 */
void word_list_push(word_list* l, word_node* n) {
    if (!l || !n) return;
    if (l->last) l->last->next = n;
    else l->first = n;
    l->last = n;
    l->size++;
}

/**
 * @brief Appends a line node to a line list
 *
 * The node is appended at the end of the list.
 *
 * @param l Target line list
 * @param n Line node to append
 */
void line_list_push(line_list* l, line_node* n) {
    if (!l || !n) return;
    if (l->last) l->last->next = n;
    else l->first = n;
    l->last = n;
    l->size++;
}

/**
 * @brief Retrieves a word node by index
 *
 * Traverses the list sequentially until the requested index is reached.
 *
 * @param l Word list
 * @param i Zero-based index
 * @return Pointer to the word_node, or NULL if out of range
 */
word_node* word_list_get(word_list* l, size_t i) {
    if (!l) return NULL;
    word_node* h = l->first;
    for (size_t j = 0; j < i && h; j++) h = h->next;
    return h;
}

/**
 * @brief Retrieves a line node by index
 *
 * Traverses the list sequentially until the requested index is reached.
 *
 * @param l Line list
 * @param i Zero-based index
 * @return Pointer to the line_node, or NULL if out of range
 */
line_node* line_list_get(line_list* l, size_t i) {
    if (!l) return NULL;
    line_node* h = l->first;
    for (size_t j = 0; j < i && h; j++) h = h->next;
    return h;
}

/**
 * @brief Splits a single line into words
 *
 * Parses a line buffer and splits it into words separated by spaces.
 *
 * Special behavior:
 *   - The word pool is reset for each line.
 *   - If the first word is exactly "STRING", parsing stops immediately.
 *     The remainder of the line is treated as literal text.
 *
 * This function performs no dynamic allocation.
 *
 * @param str Pointer to the line buffer
 * @param len Length of the line
 * @return Pointer to a word_list containing parsed words
 */
word_list* parse_words(const char* str, size_t len) {
    // Reset of the word pool PER LINE!!!
    static uint8_t nextWord;
    nextWord = 0;
    
    word_list* l = word_list_create();
    if (len == 0) return l;

    size_t j = 0;
    bool first_word = true;
    
    for (size_t i = 0; i <= len; i++) {
        if (i == len || str[i] == ' ') {
            size_t k = i - j;
            
            if (k > 0) {
                // Check for overflow
                if (nextWord >= MAX_WORD_NODES) break;
                
                word_node* n = &wordPool[nextWord++];
                n->str = &str[j];
                n->len = k;
                n->next = NULL;
                
                word_list_push(l, n);
                
                // BYPASS STRING
                if (first_word && k == 6 && 
                    str[j]=='S' && str[j+1]=='T' && str[j+2]=='R' && 
                    str[j+3]=='I' && str[j+4]=='N' && str[j+5]=='G') {
                    return l;
                }
                first_word = false;
            }
            j = i + 1;
        }
    }
    
    return l;
}

/**
 * @brief Splits a buffer into individual lines
 *
 * Scans the input buffer for line breaks ('\\r' or '\\n') and creates
 * a line node for each non-empty line.
 *
 * Each line is further processed by parse_words().
 * Static memory pools are reset at the beginning of the call.
 *
 * @param str Pointer to the input buffer
 * @param len Number of valid bytes in the buffer
 * @return Pointer to a line_list containing parsed lines
 */
line_list* parse_lines(const char* str, size_t len) {
    reset_pools();
    
    line_list* l = line_list_create();
    if (len == 0) return l;

    size_t ls = 0;
    for (size_t stri = 0; stri <= len; stri++) {
        bool linebreak = (str[stri] == '\r' || str[stri] == '\n');
        bool endofbuf = (stri == len);

        if (linebreak || endofbuf) {
            size_t llen = stri - ls;
            if (llen > 0) {
                line_node* n = line_node_create(&str[ls], llen);
                if (n) {
                    word_list* words = parse_words(&str[ls], llen);
                    n->words = *words;  // Copy structure
                    line_list_push(l, n);
                }
            }
            ls = stri + 1;
        }
    }
    return l;
}

#else
#include <stdlib.h>  // malloc

/**
 * @brief Converts an ASCII character to lowercase
 *
 * This is a lightweight replacement for tolower() to avoid pulling in
 * locale-dependent or heavy implementations on constrained systems.
 *
 * Only ASCII A–Z characters are converted. All other characters are returned
 * unchanged.
 *
 * @param c Input character
 * @return Lowercase character if applicable, otherwise the original character
 */
// My own implementation, because the default one in ctype.h make problems on older ESP8266 SDKs
char to_lower(char c) {
    if ((c >= 65) && (c <= 90)) {
        return (char)(c + 32);
    }
    return c;
}

/**
 * @brief Compares a user-provided string against a command template
 *
 * This function checks whether a user input string matches a template string.
 *
 * Case-sensitive and case-insensitive comparisons are supported.
 *
 * @param user_str        Pointer to the user input string
 * @param user_str_len    Length of the user input string
 * @param templ_str       Command template string
 * @param case_sensitive Comparison mode
 *
 * @return COMPARE_EQUAL if the strings match, otherwise COMPARE_UNEQUAL
 */
int compare(const char* user_str, size_t user_str_len, const char* templ_str, int case_sensetive) {
    if (user_str == templ_str) return COMPARE_EQUAL;

    // null check string pointers
    if (!user_str || !templ_str) return COMPARE_UNEQUAL;

    // string lengths
    size_t str_len = user_str_len; // strlen(user_str);
    size_t key_len = strlen(templ_str);

    // when same length, it there is no need to check for slashes or commas
    if (str_len == key_len) {
        for (size_t i = 0; i < key_len; i++) {
            if (case_sensetive == COMPARE_CASE_SENSETIVE) {
                if (user_str[i] != templ_str[i]) return COMPARE_UNEQUAL;
            } else {
                if (to_lower(user_str[i]) != to_lower(templ_str[i])) return COMPARE_UNEQUAL;
            }
        }
        return COMPARE_EQUAL;
    }

    // string can't be longer than templ_str (but can be smaller because of  '/' and ',')
    if (str_len > key_len) return COMPARE_UNEQUAL;

    unsigned int res_i = 0;
    unsigned int a     = 0;
    unsigned int b     = 0;
    unsigned int res   = 1;

    while (a < str_len && b < key_len) {
        if (templ_str[b] == '/') {
            // skip slash in templ_str
            ++b;
        } else if (templ_str[b] == ',') {
            // on comma increment res_i and reset str-index
            ++b;
            a = 0;
            ++res_i;
        }

        // compare character
        if (case_sensetive == COMPARE_CASE_SENSETIVE) {
            if (user_str[a] != templ_str[b]) res = 0;
        } else {
            if (to_lower(user_str[a]) != to_lower(templ_str[b])) res = 0;
        }

        // comparison incorrect or string checked until the end and templ_str not checked until the end
        if (!res || ((a == str_len - 1) &&
                     (templ_str[b + 1] != ',') &&
                     (templ_str[b + 1] != '/') &&
                     (templ_str[b + 1] != '\0'))) {
            // fast forward to next comma
            while (b < key_len && templ_str[b] != ',') b++;
            res = 1;
        } else {
            // otherwise icrement indices
            ++a;
            ++b;
        }
    }

    // comparison correct AND string checked until the end AND templ_str checked until the end
    if (res && (a == str_len) &&
        ((templ_str[b] == ',') ||
         (templ_str[b] == '/') ||
         (templ_str[b] == '\0'))) return COMPARE_EQUAL;  // res_i

    return COMPARE_UNEQUAL;
}

// ===== Word Node ===== //

/**
 * @brief Allocates a word node dynamically
 *
 * Allocates memory on the heap for a new word node.
 *
 * @param str Pointer to the word start in the source buffer
 * @param len Length of the word
 * @return Pointer to the allocated word_node
 */
word_node* word_node_create(const char* str, size_t len) {
    word_node* n = (word_node*)malloc(sizeof(word_node));

    n->str  = str;
    n->len  = len;
    n->next = NULL;
    return n;
}

/**
 * @brief Frees a single word node
 *
 * @param n Word node to free
 * @return Always returns NULL
 */
word_node* word_node_destroy(word_node* n) {
    if (n) {
        free(n);
    }
    return NULL;
}

/**
 * @brief Recursively frees a linked list of word nodes
 *
 * @param n First node in the list
 * @return Always returns NULL
 */
word_node* word_node_destroy_rec(word_node* n) {
    if (n) {
        word_node_destroy_rec(n->next);
        word_node_destroy(n);
    }
    return NULL;
}

// ===== Word List ===== //

/**
 * @brief Allocates and initializes a word list
 *
 * @return Pointer to a newly allocated word_list
 */
word_list* word_list_create() {
    word_list* l = (word_list*)malloc(sizeof(word_list));

    l->first = NULL;
    l->last  = NULL;
    l->size  = 0;
    return l;
}

/**
 * @brief Frees a word list and all contained word nodes
 *
 * @param l Word list to destroy
 * @return Always returns NULL
 */
word_list* word_list_destroy(word_list* l) {
    if (l) {
        word_node_destroy_rec(l->first);
        free(l);
    }
    return NULL;
}

/**
 * @brief Appends a word node to a word list
 *
 * The node is appended at the end of the list.
 *
 * @param l Target word list
 * @param n Word node to append
 */
void word_list_push(word_list* l, word_node* n) {
    if (l && n) {
        if (l->last) {
            l->last->next = n;
        } else {
            l->first = n;
        }

        l->last = n;
        ++l->size;
    }
}

/**
 * @brief Retrieves a word node by index
 *
 * Traverses the list sequentially until the requested index is reached.
 *
 * @param l Word list
 * @param i Zero-based index
 * @return Pointer to the word_node, or NULL if out of range
 */
word_node* word_list_get(word_list* l, size_t i) {
    if (!l) return NULL;

    size_t j;
    word_node* h = l->first;

    for (j = 0; j < i && h; ++j) {
        h = h->next;
    }

    return h;
}

// ===== Line Node ==== //

/**
 * @brief Allocates and initializes a line node
 *
 * @param str Pointer to the line start in the source buffer
 * @param len Length of the line
 * @return Pointer to a newly allocated line_node
 */
line_node* line_node_create(const char* str, size_t len) {
    line_node* n = (line_node*)malloc(sizeof(line_node));

    n->str   = str;
    n->len   = len;
    n->words = NULL;
    n->next  = NULL;

    return n;
}

/**
 * @brief Frees a single line node and its word list
 *
 * @param n Line node to destroy
 * @return Always returns NULL
 */
word_node* line_node_destroy(line_node* n) {
    if (n) {
        word_list_destroy(n->words);
        free(n);
    }
    return NULL;
}

/**
 * @brief Recursively frees a linked list of line nodes
 *
 * @param n First node in the list
 * @return Always returns NULL
 */
word_node* line_node_destroy_rec(line_node* n) {
    if (n) {
        line_node_destroy_rec(n->next);
        line_node_destroy(n);
    }
    return NULL;
}

// ===== Line List ===== //

/**
 * @brief Allocates and initializes a line list
 *
 * @return Pointer to a newly allocated line_list
 */
line_list* line_list_create() {
    line_list* l = (line_list*)malloc(sizeof(line_list));

    l->first = NULL;
    l->last  = NULL;
    l->size  = 0;

    return l;
}

/**
 * @brief Frees a line list and all contained line nodes
 *
 * @param l Line list to destroy
 * @return Always returns NULL
 */
line_list* line_list_destroy(line_list* l) {
    if (l) {
        line_node_destroy_rec(l->first);
        free(l);
    }
    return NULL;
}

/**
 * @brief Appends a line node to a line list
 *
 * The node is appended at the end of the list.
 *
 * @param l Target line list
 * @param n Line node to append
 */
void line_list_push(line_list* l, line_node* n) {
    if (l && n) {
        if (l->last) {
            l->last->next = n;
        } else {
            l->first = n;
        }

        l->last = n;
        ++l->size;
    }
}

/**
 * @brief Retrieves a line node by index
 *
 * Traverses the list sequentially until the requested index is reached.
 *
 * @param l Line list
 * @param i Zero-based index
 * @return Pointer to the line_node, or NULL if out of range
 */
line_node* line_list_get(line_list* l, size_t i) {
    if (!l) return NULL;

    size_t j;
    line_node* h = l->first;

    for (j = 0; j < i && h; ++j) {
        h = h->next;
    }

    return h;
}

// ===== Parser ===== //

/**
 * @brief Splits a line into words with escape and quote handling
 *
 * Supports:
 *   - Escaped characters using '\\'
 *   - Quoted strings to preserve spaces
 *
 * Words are separated by spaces unless escaped or inside quotes.
 *
 * @param str Pointer to the line buffer
 * @param len Length of the line
 * @return Pointer to a dynamically allocated word_list
 */
word_list* parse_words(const char* str, size_t len) {
    word_list* l = word_list_create();

    if (len == 0) return l;

    // Go through string and look for space to split it into words
    word_node* n = NULL;

    size_t i = 0; // current index
    size_t j = 0; // start index of word

    int escaped      = 0;
    int ignore_space = 0;

    for (i = 0; i <= len; ++i) {
        if ((str[i] == '\\') && (escaped == 0)) {
            escaped = 1;
        } else if ((str[i] == '"') && (escaped == 0)) {
            ignore_space = !ignore_space;
        } else if ((i == len) || ((str[i] == ' ') && (ignore_space == 0) && (escaped == 0))) {
            size_t k = i - j; // length of word

            // for every word, add to list
            if (k > 0) {
                n = word_node_create(&str[j], k);
                word_list_push(l, n);
            }

            j = i + 1; // reset start index of word
        } else if (escaped == 1) {
            escaped = 0;
        }
    }

    return l;
}

/**
 * @brief Splits a buffer into lines with escape handling
 *
 * Splits the input buffer on '\\r' and '\\n'.
 * Each line is further parsed into words using parse_words().
 *
 * @param str Pointer to the input buffer
 * @param len Number of valid bytes in the buffer
 * @return Pointer to a dynamically allocated line_list
 */
line_list* parse_lines(const char* str, size_t len) {
    line_list* l = line_list_create();

    if (len == 0) return l;

    // Go through string and look for \r and \n to split it into lines
    line_node* n = NULL;

    size_t stri = 0; // current index
    size_t ls   = 0; // start index of line

    bool escaped   = false;
    bool in_quotes = false;
    bool delimiter = false;
    bool linebreak = false;
    bool endofline = false;

    for (stri = 0; stri <= len; ++stri) {
        char prev = stri > 0 ? str[stri-1] : 0;
        char curr = str[stri];
        char next = str[stri+1];

        escaped = prev == '\\';

        // disabled because ducky script isn't using quotes
        // in_quotes = (curr == '"' && !escaped) ? !in_quotes : in_quotes;
        // delimiter = !in_quotes && !escaped && curr == ';' && next == ';';

        linebreak = !in_quotes && (curr == '\r' || curr == '\n');

        endofline = stri == len || curr == '\0';

        if (linebreak || endofline || delimiter) {
            size_t llen = stri - ls; // length of line

            // for every line, parse_words and add to list
            if (llen > 0) {
                n        = line_node_create(&str[ls], llen);
                n->words = parse_words(&str[ls], llen);
                line_list_push(l, n);
            }

            if (delimiter) ++stri;

            ls = stri+1; // reset start index of line
        }
    }

    return l;
}

#endif