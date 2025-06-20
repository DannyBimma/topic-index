#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*
 * topic_index – compute how much of a text is about a given topic and
 * report the top 5 most-used words (topic word + 4 others).
 *
 * Usage:
 *     topic_index <topic_word> [file]
 *
 * If no file is provided, input is read from stdin.  Only plain text
 * is processed – for other document formats the caller should convert
 * them to text (e.g. with `catdoc`, `pdftotext`, etc.) and pipe the
 * result into this program.
 *
 * The program follows the ANSI-C (C89) standard and attempts to avoid
 * memory leaks and undefined behaviour.
 */

#define INITIAL_BUF_SIZE 64
#define HASH_SIZE 10007 /* prime # buckets for word hash table */

/* A minimal stop-word list (common English words we want to ignore when
 * searching for the "top 4 other words").  The list can be extended. */
static const char *STOP_WORDS[] = {
    "a", "an", "and", "are", "as", "at", "be", "by", "for", "from", "has", "he", "in",
    "is", "it", "its", "of", "on", "that", "the", "to", "was", "were", "will", "with",
    "I", "you", "me", "my", "we", "our", "they", "their", "them", "this", "those",
    "these", "your", "yours", "his", "her", "hers", "him", "she", "who", "whom",
    "what", "which", "when", "where", "why", "how", "if", "or", "but", "not", NULL};

/* Linked list node for each unique word */
typedef struct WordEntry
{
    char *word;             /* lower-case word */
    long count;             /* total occurrences */
    long sentence_count;    /* number of sentences containing the word */
    long last_sentence_id;  /* helper to avoid double counting within a sentence */
    struct WordEntry *next; /* next in bucket list */
} WordEntry;

static WordEntry *hash_table[HASH_SIZE];

/* simple hash – djb2 */
static unsigned long hash_word(const char *s)
{
    unsigned long h = 5381UL;
    int c;
    while ((c = (unsigned char)*s++) != 0)
    {
        h = ((h << 5) + h) + (unsigned long)c; /* h*33 + c */
    }
    return h % HASH_SIZE;
}

static void *xmalloc(size_t n)
{
    void *p = malloc(n);
    if (!p)
    {
        fprintf(stderr, "topic_index: out of memory\n");
        exit(EXIT_FAILURE);
    }
    return p;
}

static int is_stop_word(const char *word)
{
    const char **sw = STOP_WORDS;
    while (*sw)
    {
        if (strcmp(*sw, word) == 0)
            return 1;
        ++sw;
    }
    return 0;
}

/* Obtain or create the word entry */
static WordEntry *get_word_entry(const char *word, long current_sentence)
{
    unsigned long h = hash_word(word);
    WordEntry *e = hash_table[h];
    while (e)
    {
        if (strcmp(e->word, word) == 0)
            break;
        e = e->next;
    }
    if (!e)
    {
        e = (WordEntry *)xmalloc(sizeof(WordEntry));
        e->word = strdup(word);
        if (!e->word)
        {
            fprintf(stderr, "topic_index: out of memory\n");
            exit(EXIT_FAILURE);
        }
        e->count = 0;
        e->sentence_count = 0;
        e->last_sentence_id = -1;
        e->next = hash_table[h];
        hash_table[h] = e;
    }

    /* update counts */
    e->count += 1;
    if (e->last_sentence_id != current_sentence)
    {
        e->sentence_count += 1;
        e->last_sentence_id = current_sentence;
    }
    return e;
}

static void free_hash_table(void)
{
    unsigned long i;
    for (i = 0; i < HASH_SIZE; ++i)
    {
        WordEntry *e = hash_table[i];
        while (e)
        {
            WordEntry *next = e->next;
            free(e->word);
            free(e);
            e = next;
        }
    }
}

/* Structure used for sorting the top words */
typedef struct
{
    WordEntry *entry;
} WordPtr;

static int cmp_wordptr_count(const void *a, const void *b)
{
    const WordPtr *wa = (const WordPtr *)a;
    const WordPtr *wb = (const WordPtr *)b;
    if (wa->entry->count < wb->entry->count)
        return 1;
    if (wa->entry->count > wb->entry->count)
        return -1;
    return 0;
}

int main(int argc, char *argv[])
{
    FILE *fp = NULL;
    const char *topic = NULL;

    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <topic_word> [file]\n", argv[0]);
        return EXIT_FAILURE;
    }

    topic = argv[1];

    if (argc >= 3)
    {
        fp = fopen(argv[2], "r");
        if (!fp)
        {
            perror("fopen");
            return EXIT_FAILURE;
        }
    }
    else
    {
        fp = stdin;
    }

    /* Processing variables */
    int c;
    char *buf = (char *)xmalloc(INITIAL_BUF_SIZE);
    size_t buf_cap = INITIAL_BUF_SIZE;
    size_t buf_len = 0;

    long total_words = 0;
    long total_sentences = 0;
    long current_sentence_id = 0;

    while ((c = fgetc(fp)) != EOF)
    {
        if (isalnum(c))
        {
            /* add char to buffer */
            if (buf_len + 1 >= buf_cap)
            {
                buf_cap *= 2;
                buf = (char *)realloc(buf, buf_cap);
                if (!buf)
                {
                    fprintf(stderr, "topic_index: out of memory\n");
                    exit(EXIT_FAILURE);
                }
            }
            buf[buf_len++] = (char)tolower(c);
        }
        else
        {
            /* Non-word character: finish word if buffer not empty */
            if (buf_len > 0)
            {
                buf[buf_len] = '\0';
                get_word_entry(buf, current_sentence_id);
                total_words += 1;
                buf_len = 0;
            }
            /* Sentence boundary? */
            if (c == '.' || c == '!' || c == '?')
            {
                total_sentences += 1;
                current_sentence_id = total_sentences; /* next sentence id */
            }
        }
    }
    /* flush last buffered word */
    if (buf_len > 0)
    {
        buf[buf_len] = '\0';
        get_word_entry(buf, current_sentence_id);
        total_words += 1;
        buf_len = 0;
    }
    free(buf);

    if (fp != stdin)
        fclose(fp);

    if (total_sentences == 0 && total_words > 0)
    {
        /* treat entire text as one sentence if no terminator found */
        total_sentences = 1;
    }

    /* Collect word pointers for sorting */
    WordPtr *array = NULL;
    size_t array_size = 0;
    size_t i;
    for (i = 0; i < HASH_SIZE; ++i)
    {
        WordEntry *e = hash_table[i];
        while (e)
        {
            array = (WordPtr *)realloc(array, (array_size + 1) * sizeof(WordPtr));
            if (!array)
            {
                fprintf(stderr, "topic_index: out of memory\n");
                exit(EXIT_FAILURE);
            }
            array[array_size].entry = e;
            array_size += 1;
            e = e->next;
        }
    }

    qsort(array, array_size, sizeof(WordPtr), cmp_wordptr_count);

    /* Prepare topic word lower-cased */
    char *topic_lc = strdup(topic);
    if (!topic_lc)
    {
        fprintf(stderr, "topic_index: out of memory\n");
        exit(EXIT_FAILURE);
    }
    {
        char *p = topic_lc;
        while (*p)
        {
            *p = (char)tolower((unsigned char)*p);
            ++p;
        }
    }

    /* Find topic entry */
    WordEntry *topic_entry = NULL;
    {
        unsigned long h = hash_word(topic_lc);
        WordEntry *e = hash_table[h];
        while (e)
        {
            if (strcmp(e->word, topic_lc) == 0)
            {
                topic_entry = e;
                break;
            }
            e = e->next;
        }
    }

    /* Gather top 4 non-stop-words excluding topic */
    WordEntry *top4[4] = {NULL, NULL, NULL, NULL};
    size_t found = 0;
    for (i = 0; i < array_size && found < 4; ++i)
    {
        WordEntry *e = array[i].entry;
        if (e == topic_entry)
            continue; /* skip topic */
        if (is_stop_word(e->word))
            continue; /* skip stop words */
        top4[found++] = e;
    }

    /* Output report */
    printf("=============================\n");
    printf("Topic index report\n");
    printf("Topic word: '%s'\n", topic);
    printf("Total words: %ld\n", total_words);
    printf("Total sentences: %ld\n", total_sentences);
    printf("=============================\n");
    printf("%-15s %8s %10s %15s %10s\n", "Word", "Count", "% Words", "Sentences", "% Sent");
    printf("-------------------------------------------------------------------\n");

    /* helper macro for printing line */
#define PRINT_LINE(entry)                                                                            \
    do                                                                                               \
    {                                                                                                \
        if (entry)                                                                                   \
        {                                                                                            \
            double pct_w = (total_words > 0)                                                         \
                               ? (100.0 * (double)(entry)->count / (double)total_words)              \
                               : 0.0;                                                                \
            double pct_s = (total_sentences > 0)                                                     \
                               ? (100.0 * (double)(entry)->sentence_count / (double)total_sentences) \
                               : 0.0;                                                                \
            printf("%-15s %8ld %9.2f%%   %5ld/%-7ld %8.2f%%\n",                                      \
                   (entry)->word, (entry)->count, pct_w,                                             \
                   (entry)->sentence_count, total_sentences, pct_s);                                 \
        }                                                                                            \
    } while (0)

    PRINT_LINE(topic_entry);
    for (i = 0; i < 4; ++i)
    {
        PRINT_LINE(top4[i]);
    }

    printf("=============================\n");

    /* cleanup */
    free(array);
    free(topic_lc);
    free_hash_table();

    return EXIT_SUCCESS;
}