/*{{{  includes */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

#ifdef WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "_aterm.h"
#include "memory.h"
#include "afun.h"
#include "list.h"
#include "gc.h"
#include "util.h"
#include "bafio.h"
#include "atypes.h"
#include "safio.h"

/*}}}  */

namespace aterm
{

/*{{{  defines */

#define MCRL2_LOW_MEMORY_FLAG "-at-low-memory"

static const size_t DEFAULT_BUFFER_SIZE = 4096;

static const int ERROR_SIZE = 32;
static const size_t INITIAL_MARK_STACK_SIZE = 16384;
static const size_t MARK_STACK_MARGE = AT_getMaxTermSize();

/* Initial number of terms that can be protected */
/* In the current implementation this means that
   excessive term protection can lead to deteriorating
   performance! */
static const size_t INITIAL_PROT_TABLE_SIZE = 100003;
static const size_t PROTECT_EXPAND_SIZE = 100000;

/* The same for the protected arrays */
/* #define PROTECT_ARRAY_INITIAL_SIZE 128
#define PROTECT_ARRAY_EXPAND_SIZE  256 */

/* The same for protection function */
static const size_t PROTECT_FUNC_INITIAL_SIZE = 32;
static const size_t PROTECT_FUNC_EXPAND_SIZE = 32;

/*}}}  */
/*{{{  globals */

char            aterm_id[] = "$Id: aterm.c 24415 2007-12-12 14:20:55Z eriks $";

/* Flag to tell whether to keep quiet or not. */
bool low_memory = false;

/* Flag set when ATinit is called. */
static bool initialized = false;

/* We need a buffer for printing and parsing */
static size_t buffer_size = 0;
static char*         buffer = NULL;

// prototype
static void resize_buffer(const size_t n);

inline
void RESIZE_BUFFER(const size_t n)
{
  if(n > buffer_size)
  {
    resize_buffer(n);
  }
}
//#define RESIZE_BUFFER(n) if(n > buffer_size) resize_buffer(n)

/* Parse error description */
static int      line = 0;
static int      col = 0;
static char     error_buf[ERROR_SIZE];
static int      error_idx = 0;

ProtEntry*       free_prot_entries = NULL;
ProtEntry**       at_prot_table = NULL;
size_t    at_prot_table_size = 0;
ProtEntry*       at_prot_memory = NULL;
ATermProtFunc*   at_prot_functions = NULL;
size_t    at_prot_functions_size = 0;
size_t    at_prot_functions_count = 0;

static ATerm*   mark_stack = NULL;
static size_t mark_stack_size = 0;

/*}}}  */
/*{{{  function declarations */

#if !(defined __USE_SVID || defined __USE_BSD || defined __USE_XOPEN_EXTENDED || defined __APPLE__ || defined _MSC_VER)
extern char* _strdup(const char* s);
#endif

static ATerm    fparse_term(int* c, FILE* f);
static ATerm    sparse_term(int* c, char** s);
/* static ATerm AT_diff(ATerm t1, ATerm t2, ATermList *diffs); */

/*}}}  */

/*{{{  void AT_cleanup() */

/**
 * Perform necessary cleanup.
 */

void
AT_cleanup(void)
{
  AT_cleanupMemory();
}

/*}}}  */

/*{{{  void ATinit(int argc, char *argv[], ATerm *bottomOfStack) */

/**
 * Initialize the ATerm library.
 */

void
ATinit(int argc, char* argv[], ATerm* bottomOfStack)
{
  if (initialized)
  {
    return;
  }

  /*{{{  Optionally print some information */

  AT_init_gc_parameters(low_memory);

  /*}}}  */
  /*{{{  Perform some sanity checks */

  /* Protect novice users that simply pass NULL as bottomOfStack */
  if (bottomOfStack == NULL)
  {
    ATerror("ATinit: illegal bottomOfStack (arg 3) passed.\n");
  }

  /* Check for reasonably sized ATerm (32 bits, 4 bytes)     */
  /* This check might break on perfectly valid architectures */
  /* that have char == 2 bytes, and sizeof(header_type) == 2 */
  assert(sizeof(header_type) == sizeof(ATerm*));
  assert(sizeof(header_type) >= 4);

  /*}}}  */
  /*{{{  Initialize buffer */

  buffer_size = DEFAULT_BUFFER_SIZE;
  buffer = (char*) AT_malloc(DEFAULT_BUFFER_SIZE);
  if (!buffer)
    ATerror("ATinit: cannot allocate string buffer of size %d\n",
            DEFAULT_BUFFER_SIZE);

  /*}}}  */
  /*{{{  Initialize protected terms */

  at_prot_table_size = INITIAL_PROT_TABLE_SIZE;
  at_prot_table = (ProtEntry**)AT_calloc(at_prot_table_size, sizeof(ProtEntry*));
  if (!at_prot_table)
    ATerror("ATinit: cannot allocate space for prot-table of size %ud\n",
            at_prot_table_size);

  /*}}}  */
  /*{{{  Initialize mark stack */

  /* Allocate initial mark stack */
  mark_stack = (ATerm*) AT_malloc(sizeof(ATerm) * INITIAL_MARK_STACK_SIZE);
  if (!mark_stack)
    ATerror("cannot allocate marks stack of %ud entries.\n",
            INITIAL_MARK_STACK_SIZE);
  mark_stack_size = INITIAL_MARK_STACK_SIZE;

  /*}}}  */
  /*{{{  Initialize other components */

  /* Initialize other components */
  AT_initMemory(argc, argv);
  AT_initAFun(argc, argv);
  AT_initList(argc, argv);
  AT_initGC(argc, argv, bottomOfStack);
  AT_initBafIO(argc, argv);

  /*}}}  */

#ifdef WIN32
  if (_setmode(_fileno(stdin), _O_BINARY) == -1)
  {
    perror("Warning: Cannot set inputstream to binary mode.");
  }
  if (_setmode(_fileno(stdout), _O_BINARY) == -1)
  {
    perror("Warning: Cannot set outputstream to binary mode.");
  }
  if (_setmode(_fileno(stderr), _O_BINARY) == -1)
  {
    perror("Warning: Cannot set errorstream to binary mode.");
  }
#endif

  initialized = true;

  atexit(AT_cleanup);
}

bool ATisInitialized()
{
  return initialized;
}

/*}}}  */

/*{{{  void ATwarning(const char *format, ...) */

void
ATwarning(const char* format,...)
{
  va_list args;

  va_start(args, format);
  ATvfprintf(stderr, format, args);

  va_end(args);
}

/*}}}  */
/*{{{  void ATerror(const char *format, ...) */

/**
 * A fatal error was detected.
 */

void
ATerror(const char* format,...)
{
  va_list         args;

  va_start(args, format);
  ATvfprintf(stderr, format, args);
  assert(0);
  exit(1);
  va_end(args);
}

/*}}}  */
/*{{{  void ATabort(const char *format, ...) */

/**
 * A fatal error was detected.
 */

void
ATabort(const char* format,...)
{
  va_list         args;

  va_start(args, format);
  ATvfprintf(stderr, format, args);
  abort();

  va_end(args);
}

/*}}}  */

/*{{{  void ATprotect(ATerm *term) */

/**
 * Protect a given term.
 */

void
ATprotect(ATerm* term)
{
  ATprotectArray(term, 1);
}

/*}}}  */
/*{{{  void ATunprotect(ATerm *term) */

/**
 * Unprotect a given term.
 */

void
ATunprotect(ATerm* term)
{
  ATunprotectArray(term);
}

/*}}}  */
/*{{{  void ATprotectArray(ATerm *start, int size) */

/**
 * Protect an array
 */

void ATprotectArray(ATerm* start, size_t size)
{
  ProtEntry* entry;
  ShortHashNumber hnr;
  size_t i;

#ifndef NDEBUG
  for (i=0; i<size; i++)
  {
    assert(start[i] == NULL || AT_isValidTerm(start[i])); /* Check the precondition */
  }
#endif

  if (!free_prot_entries)
  {
    ProtEntry* entries = (ProtEntry*)AT_calloc(PROTECT_EXPAND_SIZE, sizeof(ProtEntry));
    if (!entries)
    {
      ATerror("out of memory in ATprotect.\n");
    }

    for (i=0; i<PROTECT_EXPAND_SIZE; i++)
    {
      entries[i].next = free_prot_entries;
      free_prot_entries = &entries[i];
    }
  }
  entry = free_prot_entries;
  free_prot_entries = free_prot_entries->next;
  hnr = ADDR_TO_SHORT_HNR(start);

  hnr %= at_prot_table_size;
  entry->next = at_prot_table[hnr];
  at_prot_table[hnr] = entry;
  entry->start = start;
  entry->size  = size;
}

/*}}}  */
/*{{{  void ATunprotectArrray(ATerm *start) */

/**
 * Unprotect an array of terms.
 */

void ATunprotectArray(ATerm* start)
{
  ShortHashNumber hnr;
  ProtEntry* entry, *prev;

  hnr = ADDR_TO_SHORT_HNR(start);
  hnr %= at_prot_table_size;
  entry = at_prot_table[hnr];

  prev = NULL;
  while (entry->start != start)
  {
    prev  = entry;
    entry = entry->next;
    assert(entry);
  }

  if (prev)
  {
    prev->next = entry->next;
  }
  else
  {
    at_prot_table[hnr] = entry->next;
  }

  entry->next = free_prot_entries;
  free_prot_entries = entry;
}

/*}}}  */

void ATaddProtectFunction(ATermProtFunc f)
{
  ATermProtFunc* new_at_prot_functions;
  size_t old_at_prot_functions_size = at_prot_functions_size;

  if (at_prot_functions_count == at_prot_functions_size)
  {
    if (!at_prot_functions)
    {
      at_prot_functions_size = PROTECT_FUNC_INITIAL_SIZE;
      new_at_prot_functions = (ATermProtFunc*) AT_malloc(at_prot_functions_size*sizeof(ATermProtFunc));
    }
    else
    {
      if (low_memory)
      {
        at_prot_functions_size += 1;
      }
      else
      {
        at_prot_functions_size += PROTECT_FUNC_EXPAND_SIZE;
      }

      new_at_prot_functions = (ATermProtFunc*) AT_realloc(at_prot_functions, at_prot_functions_size*sizeof(ATermProtFunc));

      /* Reallocation failed again; try with a single extra element */
      if ((!new_at_prot_functions) && (!low_memory))
      {
        at_prot_functions_size = old_at_prot_functions_size + 1;
        new_at_prot_functions = (ATermProtFunc*) AT_realloc(at_prot_functions, at_prot_functions_size*sizeof(ATermProtFunc));
      }
    }

    if (!new_at_prot_functions)
    {
      ATerror("out of memory in ATaddProtectFunction.\n");
    }
    else
    {
      at_prot_functions = new_at_prot_functions;
    }
  }

  at_prot_functions[at_prot_functions_count++] = f;
}

/*}}}  */

/*{{{  int ATprintf(const char *format, ...) */

/**
 * Extension of printf() with ATerm-support.
 */

int
ATprintf(const char* format,...)
{
  int             result = 0;
  va_list         args;

  va_start(args, format);
  result = ATvfprintf(stdout, format, args);
  va_end(args);

  return result;
}

/*}}}  */
/*{{{  int ATfprintf(FILE *stream, const char *format, ...) */

/**
 * Extension of fprintf() with ATerm-support.
 */

int
ATfprintf(FILE* stream, const char* format,...)
{
  int             result = 0;
  va_list         args;

  va_start(args, format);
  result = ATvfprintf(stream, format, args);
  va_end(args);

  return result;
}
/*}}}  */
/*{{{  int ATvfprintf(FILE *stream, const char *format, va_list args) */

int
ATvfprintf(FILE* stream, const char* format, va_list args)
{
  const char*     p;
  char*           s;
  char            fmt[16];
  int             result = 0;
  ATerm           t;
  ATermList       l;

  for (p = format; *p; p++)
  {
    if (*p != '%')
    {
      fputc(*p, stream);
      continue;
    }

    s = fmt;
    while (!isalpha((int) *p))  /* parse formats %-20s, etc. */
    {
      *s++ = *p++;
    }
    *s++ = *p;
    *s = '\0';

    switch (*p)
    {
      case 'c':
      case 'd':
      case 'i':
      case 'o':
      case 'u':
      case 'x':
      case 'X':
        fprintf(stream, fmt, va_arg(args, int));
        break;

      case 'e':
      case 'E':
      case 'f':
      case 'g':
      case 'G':
        fprintf(stream, fmt, va_arg(args, double));
        break;

      case 'p':
        fprintf(stream, fmt, va_arg(args, void*));
        break;

      case 's':
        fprintf(stream, fmt, va_arg(args, char*));
        break;

        /*
         * ATerm specifics start here: "%t" to print an ATerm; "%l" to
         * print a list; "%y" to print a AFun; "%n" to print a single
         * ATerm node
         */
      case 't':
        ATwriteToTextFile(va_arg(args, ATerm), stream);
        break;
      case 'l':
        l = va_arg(args, ATermList);
        fmt[strlen(fmt) - 1] = '\0';  /* Remove 'l' */
        while (!ATisEmpty(l))
        {
          ATwriteToTextFile(ATgetFirst(l), stream);
          /*
           * ATfprintf(stream, "\nlist node: %n\n", l);
           * ATfprintf(stream, "\nlist element: %n\n", ATgetFirst(l));
           */
          l = ATgetNext(l);
          if (!ATisEmpty(l))
          {
            fputs(fmt + 1, stream);
          }
        }
        break;
      case 'a':
      case 'y':
        AT_printAFun(va_arg(args, AFun), stream);
        break;
      case 'n':
        t = va_arg(args, ATerm);
        switch (ATgetType(t))
        {
          case AT_INT:
          case AT_LIST:
            fprintf(stream, "[...(%lu)]", ATgetLength((ATermList) t));
            break;

          case AT_APPL:
            if (AT_isValidAFun(ATgetAFun((ATermAppl)t)))
            {
              AT_printAFun(ATgetAFun((ATermAppl)t), stream);
              fprintf(stream, "(...(%lu))",
                      GET_ARITY(t->header));
            }
            else
            {
              fprintf(stream, "<sym>(...(%lu))",
                      GET_ARITY(t->header));
            }
            break;
          case AT_FREE:
            fprintf(stream, "@");
            break;
          default:
            fprintf(stream, "#");
            break;
        }
        break;

      default:
        fputc(*p, stream);
        break;
    }
  }
  return result;
}

/*}}}  */

/*{{{  static void resize_buffer(int n) */

/**
 * Resize the resident string buffer
 */

static void
resize_buffer(size_t n)
{
  buffer_size = n;
  buffer = (char*) AT_realloc(buffer, buffer_size);
  if (!buffer)
  {
    ATerror("resize_buffer(aterm.c): cannot allocate string buffer of size %d\n", buffer_size);
  }
}

/*}}}  */

/*{{{  ATbool ATwriteToTextFile(ATerm t, FILE *f) */

/**
 * Write a term in text format to file.
 */

bool
writeToTextFile(ATerm t, FILE* f)
{
  AFun          sym;
  ATerm           arg;
  size_t    i, arity; /* size; */
  ATermAppl       appl;
  ATermList       list;
  /* ATermBlob       blob; */
  char*            name;

  switch (ATgetType(t))
  {
    case AT_INT:
      fprintf(f, "%d", ATgetInt((ATermInt)t));
      break;
    case AT_APPL:
      /*{{{  Print application */

      appl = (ATermAppl) t;

      sym = ATgetAFun(appl);
      AT_printAFun(sym, f);
      arity = ATgetArity(sym);
      name = ATgetName(sym);
      if (arity > 0 || (!ATisQuoted(sym) && *name == '\0'))
      {
        fputc('(', f);
        for (i = 0; i < arity; i++)
        {
          if (i != 0)
          {
            fputc(',', f);
          }
          arg = ATgetArgument(appl, i);
          ATwriteToTextFile(arg, f);
        }
        fputc(')', f);
      }

      /*}}}  */
      break;
    case AT_LIST:
      /*{{{  Print list */

      list = (ATermList) t;
      if (!ATisEmpty(list))
      {
        ATwriteToTextFile(ATgetFirst(list), f);
        list = ATgetNext(list);
      }
      while (!ATisEmpty(list))
      {
        fputc(',', f);
        ATwriteToTextFile(ATgetFirst(list), f);
        list = ATgetNext(list);
      }

      /*}}}  */
      break;
    case AT_FREE:
      if (AT_inAnyFreeList(t))
      {
        ATerror("ATwriteToTextFile: printing free term at %p!\n", t);
      }
      else
      {
        ATerror("ATwriteToTextFile: free term %p not in freelist?\n", t);
      }
      return false;

    case AT_SYMBOL:
      ATerror("ATwriteToTextFile: not a term but an afun: %y\n", t);
      return false;
  }

  return true;
}

bool
ATwriteToTextFile(ATerm t, FILE* f)
{
  bool result = true;

  if (ATgetType(t) == AT_LIST)
  {
    fputc('[', f);

    if (!ATisEmpty((ATermList) t))
    {
      result = writeToTextFile(t, f);
    }

    fputc(']', f);
  }
  else
  {
    result = writeToTextFile(t, f);
  }

  return result;
}

/*}}}  */

/*{{{  ATerm ATwriteToNamedTextFile(char *name) */

/**
 * Write an ATerm to a named plaintext file
 */

bool ATwriteToNamedTextFile(ATerm t, const char* name)
{
  FILE*  f;
  bool result;

  if (!strcmp(name, "-"))
  {
    return ATwriteToTextFile(t, stdout);
  }

  if (!(f = fopen(name, "wb")))
  {
    return false;
  }

  result = ATwriteToTextFile(t, f);
  fclose(f);

  return result;
}

/*{{{  static int symbolTextSize(AFun sym) */

/**
 * Calculate the size of a symbol in text format.
 */

static size_t
symbolTextSize(AFun sym)
{
  char*           id = ATgetName(sym);

  if (ATisQuoted(sym))
  {
    int             len = 2;
    while (*id)
    {
      /* We need to escape special characters */
      switch (*id)
      {
        case '\\':
        case '"':
        case '\n':
        case '\t':
        case '\r':
          len += 2;
          break;
        default:
          len++;
      }
      id++;
    }
    return len;
  }
  else
  {
    return strlen(id);
  }
}

/*}}}  */
/*{{{  static char *writeAFunToString(AFun sym, char *buf) */

/**
 * Write a symbol in a string buffer.
 */

static char*
writeAFunToString(AFun sym, char* buf)
{
  char*           id = ATgetName(sym);

  if (ATisQuoted(sym))
  {
    *buf++ = '"';
    while (*id)
    {
      /* We need to escape special characters */
      switch (*id)
      {
        case '\\':
        case '"':
          *buf++ = '\\';
          *buf++ = *id;
          break;
        case '\n':
          *buf++ = '\\';
          *buf++ = 'n';
          break;
        case '\t':
          *buf++ = '\\';
          *buf++ = 't';
          break;
        case '\r':
          *buf++ = '\\';
          *buf++ = 'r';
          break;
        default:
          *buf++ = *id;
      }
      id++;
    }
    *buf++ = '"';
    return buf;
  }
  else
  {
    strcpy(buf, id);
    return buf + strlen(buf);
  }
}

/*}}}  */
/*{{{  static char *writeToString(ATerm t, char *buf) */

static char*    topWriteToString(ATerm t, char* buf);

static char*
writeToString(ATerm t, char* buf)
{
  ATermList list;
  ATermAppl appl;
  AFun sym;
  size_t i, arity;
  char* name;

  switch (ATgetType(t))
  {
    case AT_INT:
      /*{{{  write integer */

      sprintf(buf, "%d", ATgetInt((ATermInt) t));
      buf += strlen(buf);

      /*}}}  */
      break;

    case AT_APPL:
      /*{{{  write appl */

      appl = (ATermAppl) t;
      sym = ATgetAFun(appl);
      arity = ATgetArity(sym);
      name = ATgetName(sym);
      buf = writeAFunToString(sym, buf);
      if (arity > 0 || (!ATisQuoted(sym) && *name == '\0'))
      {
        *buf++ = '(';
        if (arity > 0)
        {
          buf = topWriteToString(ATgetArgument(appl, 0), buf);
          for (i = 1; i < arity; i++)
          {
            *buf++ = ',';
            buf = topWriteToString(ATgetArgument(appl, i), buf);
          }
        }
        *buf++ = ')';
      }

      /*}}}  */
      break;

    case AT_LIST:
      /*{{{  write list */

      list = (ATermList) t;
      if (!ATisEmpty(list))
      {
        buf = topWriteToString(ATgetFirst(list), buf);
        list = ATgetNext(list);
        while (!ATisEmpty(list))
        {
          *buf++ = ',';
          buf = topWriteToString(ATgetFirst(list), buf);
          list = ATgetNext(list);
        }
      }

      /*}}}  */
      break;

  }
  return buf;
}

static char*
topWriteToString(ATerm t, char* buf)
{

  if (ATgetType(t) == AT_LIST)
  {
    *buf++ = '[';
    buf = writeToString(t, buf);
    *buf++ = ']';
  }
  else
  {
    buf = writeToString(t, buf);
  }

  return buf;
}

/*}}}  */
/**
 * Calculate the size of a term in text format
 */

static size_t  topTextSize(ATerm t);

/*{{{  static size_t textSize(ATerm t) */

static size_t textSize(ATerm t)
{
  char numbuf[32];
  /* ATerm trm; */
  ATermList list;
  ATermAppl appl;
  AFun sym;
  size_t size;
  size_t i, arity;
  char* name;

  switch (ATgetType(t))
  {
    case AT_INT:
      sprintf(numbuf, "%d", ATgetInt((ATermInt) t));
      size = strlen(numbuf);
      break;

    case AT_APPL:
      appl = (ATermAppl) t;
      sym = ATgetAFun(appl);
      arity = ATgetArity(sym);
      name = ATgetName(sym);
      size = symbolTextSize(sym);
      for (i = 0; i < arity; i++)
      {
        size += topTextSize(ATgetArgument(appl, i));
      }
      if (arity > 0 || (!ATisQuoted(sym) && *name == '\0'))
      {
        /* Add space for the ',' characters */
        if (arity > 1)
        {
          size += arity - 1;
        }
        /* and for the '(' and ')' characters */
        size += 2;
      }
      break;

    case AT_LIST:
      list = (ATermList) t;
      if (ATisEmpty(list))
      {
        size = 0;
      }
      else
      {
        size = ATgetLength(list) - 1; /* Space for the ','
           * characters */
        while (!ATisEmpty(list))
        {
          size += topTextSize(ATgetFirst(list));
          list = ATgetNext(list);
        }
      }
      break;

    default:
      ATerror("textSize: Illegal type %d\n", ATgetType(t));
      return (size_t)-1; // error path
  }
  return size;
}

/*}}}  */

static size_t
topTextSize(ATerm t)
{
  size_t size = textSize(t);

  if (ATgetType(t) == AT_LIST)
  {
    size += 2; /* For markers on both sides of the term */
  }

  return size;
}

/* size_t
AT_calcTextSize(ATerm t)
{
  return topTextSize(t);
} */

/**
 * Write a term into its text representation.
 */

char*
ATwriteToString(ATerm t)
{
  size_t size = topTextSize(t)+1;
  char* end;

  RESIZE_BUFFER(size);

  end = topWriteToString(t, buffer);
  *end++ = '\0';

  assert(((size_t)(end - buffer)) == size);

  return buffer;
}


/**
 * Write the text representation of a term into a buffer.
 */

void
AT_writeToStringBuffer(ATerm t, char* buffer)
{
  topWriteToString(t, buffer);
}

/*}}}  */

/*{{{  static void store_char(int char) */

/**
 * Store a single character in the buffer
 */

static void
store_char(int c, size_t pos)
{
  if (pos >= buffer_size)
  {
    resize_buffer(buffer_size * 2);  /* Double the space */
  }

  buffer[pos] = c;
}

/*}}}  */
/*{{{  static void fnext_char(int *c, FILE *f) */

/**
 * Read the next character from file.
 */

static void
fnext_char(int* c, FILE* f)
{
  *c = fgetc(f);
  if (*c != EOF)
  {
    if (*c == '\n')
    {
      line++;
      col = 0;
    }
    else
    {
      col++;
    }
    error_buf[error_idx++] = *c;
    error_idx %= ERROR_SIZE;
  }
}

/*}}}  */
/*{{{  static void fskip_layout(int *c, FILE *f) */

/**
 * Skip layout from file.
 */

static void
fskip_layout(int* c, FILE* f)
{
  while (isspace(*c))
  {
    fnext_char(c, f);
  }
}

/*}}}  */
/*{{{  static void fnext_skip_layout(int *c, FILE *f) */

/**
 * Skip layout from file.
 */

static void
fnext_skip_layout(int* c, FILE* f)
{
  do
  {
    fnext_char(c, f);
  }
  while (isspace(*c));
}

/*}}}  */

/*{{{  static ATermList fparse_terms(int *c, FILE *f) */

/**
 * Parse a list of arguments.
 */

ATermList
fparse_terms(int* c, FILE* f)
{
  ATermList list;
  ATerm el = fparse_term(c, f);

  if (el == NULL)
  {
    return NULL;
  }

  list = ATinsert(ATempty, el);

  while (*c == ',')
  {
    fnext_skip_layout(c, f);
    el = fparse_term(c, f);
    if (el == NULL)
    {
      return NULL;
    }
    list = ATinsert(list, el);
  }

  return ATreverse(list);
}

/*}}}  */
/*{{{  static ATermAppl fparse_quoted_appl(int *c, FILE *f) */

/**
 * Parse a quoted application.
 */

static ATerm
fparse_quoted_appl(int* c, FILE* f)
{
  int             len = 0;
  ATermList       args = ATempty;
  AFun          sym;
  char*           name;

  /* First parse the identifier */
  fnext_char(c, f);

  assert(*c!=STRING_MARK);

  while (*c != '"')
  {
    switch (*c)
    {
      case EOF:
        return NULL;
      case '\\':
        fnext_char(c, f);
        if (*c == EOF)
        {
          return NULL;
        }
        switch (*c)
        {
          case 'n':
            store_char('\n', len++);
            break;
          case 'r':
            store_char('\r', len++);
            break;
          case 't':
            store_char('\t', len++);
            break;
          default:
            store_char(*c, len++);
            break;
        }
        break;
      default:
        store_char(*c, len++);
        break;
    }
    fnext_char(c, f);
  }

  store_char('\0', len);

  name = _strdup(buffer);
  if (!name)
  {
    ATerror("fparse_quoted_appl: symbol too long.");
  }

  fnext_skip_layout(c, f);

  /* Time to parse the arguments */
  if (*c == '(')
  {
    fnext_skip_layout(c, f);
    if (*c != ')')
    {
      args = fparse_terms(c, f);
    }
    else
    {
      args = ATempty;
    }
    if (args == NULL || *c != ')')
    {
      return NULL;
    }
    fnext_skip_layout(c, f);
  }

  /* Wrap up this function application */
  sym = ATmakeAFun(name, ATgetLength(args), true);
  AT_free(name);
  return (ATerm)ATmakeApplList(sym, args);
}

/*}}}  */
/*{{{  static ATermAppl fparse_unquoted_appl(int *c, FILE *f) */

/**
 * Parse a quoted application.
 */

static ATermAppl
fparse_unquoted_appl(int* c, FILE* f)
{
  int len = 0;
  AFun sym;
  ATermList args = ATempty;
  char* name = NULL;

  if (*c != '(')
  {
    /* First parse the identifier */
    while (isalnum(*c)
           || *c == '-' || *c == '_' || *c == '+' || *c == '*' || *c == '$')
    {
      store_char(*c, len++);
      fnext_char(c, f);
    }
    store_char('\0', len++);
    name = _strdup(buffer);
    if (!name)
    {
      ATerror("fparse_unquoted_appl: symbol too long.");
    }

    fskip_layout(c, f);
  }

  /* Time to parse the arguments */
  if (*c == '(')
  {
    fnext_skip_layout(c, f);
    if (*c != ')')
    {
      args = fparse_terms(c, f);
    }
    else
    {
      args = ATempty;
    }
    if (args == NULL || *c != ')')
    {
      return NULL;
    }
    fnext_skip_layout(c, f);
  }

  /* Wrap up this function application */
  sym = ATmakeAFun(name ? name : "", ATgetLength(args), false);
  if (name != NULL)
  {
    AT_free(name);
  }

  return ATmakeApplList(sym, args);
}

/*}}}  */
/*{{{  static void fparse_num(int *c, FILE *f) */

/**
 * Parse a number or blob.
 */

static ATerm
fparse_num(int* c, FILE* f)
{
  char            num[32], *ptr = num, *numend = num + 30;

  if (*c == '-')
  {
    *ptr++ = *c;
    fnext_char(c, f);
  }

  while (isdigit(*c) && ptr < numend)
  {
    *ptr++ = *c;
    fnext_char(c, f);
  }

  {
    /*{{{  An integer */

    *ptr = '\0';
    return (ATerm) ATmakeInt(atoi(num));

    /*}}}  */
  }
}

/*}}}  */
/*{{{  static ATerm fparse_term(int *c, FILE *f) */

/**
 * Parse a term from file.
 */

static ATerm
fparse_term(int* c, FILE* f)
{
  /* ATerm t, result = NULL; */
  ATerm result = NULL;

  switch (*c)
  {
    case '"':
      result = (ATerm) fparse_quoted_appl(c, f);
      break;
    case '[':
      fnext_skip_layout(c, f);
      if (*c == ']')
      {
        result = (ATerm) ATempty;
      }
      else
      {
        result = (ATerm) fparse_terms(c, f);
        if (result == NULL || *c != ']')
        {
          return NULL;
        }
      }
      fnext_skip_layout(c, f);
      break;
    default:
      if (isalpha(*c) || *c == '(')
      {
        result = (ATerm) fparse_unquoted_appl(c, f);
      }
      else if (isdigit(*c))
      {
        result = fparse_num(c, f);
      }
      else if (*c == '.' || *c == '-')
      {
        result = fparse_num(c, f);
      }
      else
      {
        result = NULL;
      }
  }

  return result;
}

/*}}}  */

/*{{{  ATerm readFromTextFile(FILE *file) */

/**
 * Read a term from a text file. The first character has been read.
 */

ATerm
readFromTextFile(int* c, FILE* file)
{
  ATerm term;
  fskip_layout(c, file);

  term = fparse_term(c, file);

  if (term)
  {
    ungetc(*c, file);
  }
  else
  {
    int i;
    fprintf(stderr, "readFromTextFile: parse error at line %d, col %d%s",
            line, col, (line||col)?":\n":"");
    for (i = 0; i < ERROR_SIZE; ++i)
    {
      char c = error_buf[(i + error_idx) % ERROR_SIZE];
      if (c)
      {
        fprintf(stderr, "%c", c);
      }
    }
    fprintf(stderr, "\n");
    fflush(stderr);
  }

  return term;
}

/*}}}  */
/*{{{  ATerm ATreadFromTextFile(FILE *file) */

/**
 * Read a term from a text file.
 */

ATerm
ATreadFromTextFile(FILE* file)
{
  int c;

  line = 0;
  col = 0;
  error_idx = 0;
  memset(error_buf, 0, ERROR_SIZE);

  fnext_char(&c, file);
  return readFromTextFile(&c, file);
}

/*}}}  */
/*{{{  ATerm ATreadFromFile(FILE *file) */

/**
 * Read an ATerm from a file that could be binary or text.
 */

ATerm ATreadFromFile(FILE* file)
{
  int c;

  fnext_char(&c, file);
  if (c == 0)
  {
    /* Might be a BAF file */
    return ATreadFromBinaryFile(file);
    /* } else if (c == START_OF_SHARED_TEXT_FILE) {
      / * Might be a shared text file * /
      return AT_readFromSharedTextFile(&c, file);   */
  }
  else if (c == SAF_IDENTIFICATION_TOKEN)
  {
    int token = ungetc(SAF_IDENTIFICATION_TOKEN, file);
    if (token != SAF_IDENTIFICATION_TOKEN)
    {
      ATerror("Unable to unget the SAF identification token.\n");
    }

    return ATreadFromSAFFile(file);
  }
  else
  {
    /* Probably a text file */
    line = 0;
    col = 0;
    error_idx = 0;
    memset(error_buf, 0, ERROR_SIZE);

    return readFromTextFile(&c, file);
  }
}

/*}}}  */
/*{{{  ATerm ATreadFromNamedFile(char *name) */

/**
 * Read an ATerm from a named file
 */

ATerm ATreadFromNamedFile(const char* name)
{
  FILE*  f;
  ATerm t;

  if (!strcmp(name, "-"))
  {
    return ATreadFromFile(stdin);
  }

  if (!(f = fopen(name, "rb")))
  {
    return NULL;
  }

  t = ATreadFromFile(f);
  fclose(f);

  return t;
}

/*}}}  */

inline
void snext_char(int* c, char** s)
{
  *c = (unsigned char)*(*s)++;
}
//#define snext_char(c,s) ((*c) = ((unsigned char)*(*s)++))

inline
void sskip_layout(int* c, char** s)
{
  while (isspace(*c))
  {
    snext_char(c,s);
  }
}
//#define sskip_layout(c,s) while(isspace(*c)) snext_char(c,s)

inline
void snext_skip_layout(int* c, char** s)
{
  do
  {
    snext_char(c, s);
  } while(isspace(*c));
}
//#define snext_skip_layout(c,s) do { snext_char(c, s); } while(isspace(*c))

/*{{{  static ATermList sparse_terms(int *c, char **s) */

/**
 * Parse a list of arguments.
 */

ATermList
sparse_terms(int* c, char** s)
{
  ATermList list;
  ATerm el = sparse_term(c, s);

  if (el == NULL)
  {
    return NULL;
  }

  list = ATinsert(ATempty, el);

  while (*c == ',')
  {
    snext_skip_layout(c, s);
    el = sparse_term(c, s);
    if (el == NULL)
    {
      return NULL;
    }
    list = ATinsert(list, el);
  }

  return ATreverse(list);
}

/*{{{  static ATermAppl sparse_quoted_appl(int *c, char **s) */

/**
 * Parse a quoted application.
 */

static ATerm
sparse_quoted_appl(int* c, char** s)
{
  int             len = 0;
  ATermList       args = ATempty;
  AFun          sym;
  char*           name;

  /* First parse the identifier */
  snext_char(c, s);

  assert(*c != STRING_MARK);

  while (*c != '"')
  {
    switch (*c)
    {
      case EOF:
        /*  case '\n':
          case '\r':
          case '\t':
          */
        return NULL;
      case '\\':
        snext_char(c, s);
        if (*c == EOF)
        {
          return NULL;
        }
        switch (*c)
        {
          case 'n':
            store_char('\n', len++);
            break;
          case 'r':
            store_char('\r', len++);
            break;
          case 't':
            store_char('\t', len++);
            break;
          default:
            store_char(*c, len++);
            break;
        }
        break;
      default:
        store_char(*c, len++);
        break;
    }
    snext_char(c, s);
  }

  store_char('\0', len);

  name = _strdup(buffer);
  if (!name)
  {
    ATerror("fparse_quoted_appl: symbol too long.");
  }

  snext_skip_layout(c, s);

  /* Time to parse the arguments */
  if (*c == '(')
  {
    snext_skip_layout(c, s);
    if (*c != ')')
    {
      args = sparse_terms(c, s);
    }
    else
    {
      args = ATempty;
    }
    if (args == NULL || *c != ')')
    {
      return NULL;
    }
    snext_skip_layout(c, s);
  }

  /* Wrap up this function application */
  sym = ATmakeAFun(name, ATgetLength(args), true);
  AT_free(name);
  return (ATerm)ATmakeApplList(sym, args);
}

/*}}}  */
/*{{{  static ATermAppl sparse_unquoted_appl(int *c, char **s) */

/**
 * Parse a quoted application.
 */

static ATermAppl
sparse_unquoted_appl(int* c, char** s)
{
  int len = 0;
  AFun sym;
  ATermList args = ATempty;
  char* name = NULL;

  if (*c != '(')
  {
    /* First parse the identifier */
    while (isalnum(*c)
           || *c == '-' || *c == '_' || *c == '+' || *c == '*' || *c == '$')
    {
      store_char(*c, len++);
      snext_char(c, s);
    }
    store_char('\0', len);
    name = _strdup(buffer);
    if (!name)
    {
      ATerror("sparse_unquoted_appl: symbol too long.");
    }

    sskip_layout(c, s);
  }

  /* Time to parse the arguments */
  if (*c == '(')
  {
    snext_skip_layout(c, s);
    if (*c != ')')
    {
      args = sparse_terms(c, s);
    }
    else
    {
      args = ATempty;
    }
    if (args == NULL || *c != ')')
    {
      return NULL;
    }
    snext_skip_layout(c, s);
  }

  /* Wrap up this function application */
  sym = ATmakeAFun(name ? name : "", ATgetLength(args), false);
  if (name != NULL)
  {
    AT_free(name);
  }

  return ATmakeApplList(sym, args);
}

/*}}}  */
/*{{{  static void sparse_num(int *c, char **s) */

/**
 * Parse a number
 */

static ATerm
sparse_num(int* c, char** s)
{
  char            num[32], *ptr = num;

  if (*c == '-')
  {
    *ptr++ = *c;
    snext_char(c, s);
  }

  while (isdigit(*c))
  {
    *ptr++ = *c;
    snext_char(c, s);
  }
  {
    /*{{{  An integer */

    *ptr = '\0';
    return (ATerm) ATmakeInt(atoi(num));

    /*}}}  */
  }
}

/*}}}  */

/*{{{  static ATerm sparse_term(int *c, char **s) */

/**
 * Parse a term from file.
 */

static ATerm
sparse_term(int* c, char** s)
{
  /* ATerm t, result = NULL; */
  ATerm result = NULL;

  switch (*c)
  {
    case '"':
      result = (ATerm) sparse_quoted_appl(c, s);
      break;
    case '[':
      snext_skip_layout(c, s);
      if (*c == ']')
      {
        result = (ATerm) ATempty;
      }
      else
      {
        result = (ATerm) sparse_terms(c, s);
        if (result == NULL || *c != ']')
        {
          return NULL;
        }
      }
      snext_skip_layout(c, s);
      break;
    default:
      if (isalpha(*c) || *c == '(')
      {
        result = (ATerm) sparse_unquoted_appl(c, s);
      }
      else if (isdigit(*c))
      {
        result = sparse_num(c, s);
      }
      else if (*c == '.' || *c == '-')
      {
        result = sparse_num(c, s);
      }
      else
      {
        result = NULL;
      }
  }

  if (result != NULL)
  {
    sskip_layout(c, s);

    /*}}}  */
  }

  return result;
}

/*}}}  */
/*{{{  ATerm ATreadFromString(const char *string) */

/**
 * Read from a string.
 */

ATerm
ATreadFromString(const char* string)
{
  int             c;
  const char*     orig = string;
  ATerm           term;

  snext_skip_layout(&c, (char**) &string);

  term = sparse_term(&c, (char**) &string);

  if (term == NULL)
  {
    int i;
    fprintf(stderr, "ATreadFromString: parse error at or near:\n");
    fprintf(stderr, "%s\n", orig);
    for (i = 1; i < string - orig; ++i)
    {
      fprintf(stderr, " ");
    }
    fprintf(stderr, "^\n");
  }
  else
  {
    string--;
  }

  return term;
}

/*}}}  */

/*{{{  void AT_markTerm(ATerm t) */

/**
 * Mark a term and all of its children.
 */
void AT_markTerm(ATerm t)
{
  size_t    i, arity;
  AFun          sym;
  ATerm*          current = mark_stack + 1;
  ATerm*          limit = mark_stack + mark_stack_size - MARK_STACK_MARGE;

  mark_stack[0] = NULL;
  *current++ = t;

  while (true)
  {
    if (current >= limit)
    {
      ptrdiff_t current_index;
      current_index = current - mark_stack;

      /* We need to resize the mark stack */
      mark_stack_size = mark_stack_size * 2;
      mark_stack = (ATerm*) AT_realloc(mark_stack, sizeof(ATerm) * mark_stack_size);
      if (!mark_stack)
      {
        ATerror("cannot realloc mark stack to %lu entries.\n", mark_stack_size);
      }
      limit = mark_stack + mark_stack_size - MARK_STACK_MARGE;

      current = mark_stack + current_index;
    }

    t = *--current;

    if (!t)
    {
      if (current != mark_stack)
      {
        ATabort("AT_markTerm: premature end of mark_stack.\n");
      }
      break;
    }

    if (IS_MARKED(t->header))
    {
      continue;
    }

    SET_MARK(t->header);

    switch (GET_TYPE(t->header))
    {
      case AT_INT:
        break;

      case AT_APPL:
        sym = ATgetAFun((ATermAppl) t);

        if (AT_isValidAFun(sym))
        {
          AT_markAFun(sym);
        }
        else
        {
          continue;
        }
        arity = GET_ARITY(t->header);
        if (arity > MAX_INLINE_ARITY)
        {
          arity = ATgetArity(sym);
        }
        for (i = 0; i < arity; i++)
        {
          ATerm arg = ATgetArgument((ATermAppl) t, i);
          *current++ = arg;
        }
        break;

      case AT_LIST:
        if (!ATisEmpty((ATermList) t))
        {
          *current++ = (ATerm) ATgetNext((ATermList) t);
          *current++ = ATgetFirst((ATermList) t);
        }
        break;

    }
  }
}

void AT_markTerm_young(ATerm t)
{
  size_t    i, arity;
  AFun          sym;
  ATerm*          current = mark_stack + 1;
  ATerm*          limit = mark_stack + mark_stack_size - MARK_STACK_MARGE;

  if (IS_MARKED(t->header) || IS_OLD(t->header))
  {
    /*fprintf(stderr,"AT_markTerm_young (%p) STOP MARK: age = %d\n",t,GET_AGE(t->header));*/
    return;
  }

  mark_stack[0] = NULL;
  *current++ = t;

  while (true)
  {
    if (current >= limit)
    {
      size_t current_index;

      current_index = current - mark_stack;
      /* We need to resize the mark stack */
      mark_stack_size = mark_stack_size * 2;
      mark_stack = (ATerm*) AT_realloc(mark_stack, sizeof(ATerm) * mark_stack_size);
      if (!mark_stack)
      {
        ATerror("cannot realloc mark stack to %lu entries.\n", mark_stack_size);
      }
      limit = mark_stack + mark_stack_size - MARK_STACK_MARGE;
      fflush(stderr);

      current = mark_stack + current_index;
    }

    t = *--current;

    if (!t)
    {
      if (current != mark_stack)
      {
        ATabort("AT_markTerm: premature end of mark_stack.\n");
      }
      break;
    }
    /* TODO: optimize*/
    if (IS_MARKED(t->header) || IS_OLD(t->header))
    {
      continue;
    }

    SET_MARK(t->header);

    switch (GET_TYPE(t->header))
    {
      case AT_INT:
        break;

      case AT_APPL:
        sym = ATgetAFun((ATermAppl) t);
        /*fprintf(stderr,"AT_markTerm_young: AT_markAFun_young(%d)\n",sym);*/
        if (AT_isValidAFun(sym))
        {
          AT_markAFun_young(sym);
        }
        else
        {
          continue;
        }

        arity = GET_ARITY(t->header);
        if (arity > MAX_INLINE_ARITY)
        {
          arity = ATgetArity(sym);
        }
        for (i = 0; i < arity; i++)
        {
          ATerm arg = ATgetArgument((ATermAppl) t, i);
          *current++ = arg;
        }

        break;

      case AT_LIST:
        if (!ATisEmpty((ATermList) t))
        {
          *current++ = (ATerm) ATgetNext((ATermList) t);
          *current++ = ATgetFirst((ATermList) t);
        }
        break;

    }
  }
}

/*}}}  */
/*{{{  void AT_unmarkIfAllMarked(ATerm t) */

void AT_unmarkIfAllMarked(ATerm t)
{
  if (IS_MARKED(t->header))
  {
    /*ATfprintf(stderr, "* unmarking %t\n", t);*/
    CLR_MARK(t->header);
    switch (ATgetType(t))
    {
      case AT_INT:
        break;

      case AT_LIST:
      {
        ATermList list = (ATermList)t;
        while (!ATisEmpty(list))
        {
          CLR_MARK(list->header);
          AT_unmarkIfAllMarked(ATgetFirst(list));
          list = ATgetNext(list);
          if (!IS_MARKED(list->header))
          {
            break;
          }
        }
        CLR_MARK(list->header);
      }
      break;
      case AT_APPL:
      {
        ATermAppl appl = (ATermAppl)t;
        size_t cur_arity, cur_arg;
        AFun sym;

        sym = ATgetAFun(appl);
        AT_unmarkAFun(sym);
        cur_arity = ATgetArity(sym);
        for (cur_arg=0; cur_arg<cur_arity; cur_arg++)
        {
          AT_unmarkIfAllMarked(ATgetArgument(appl, cur_arg));
        }
      }
      break;
      default:
        ATerror("collect_terms: illegal term\n");
        break;
    }

  }
}

/*}}}  */

/**
 * Calculate the number of unique symbols.
 */

/*{{{  static size_t calcUniqueAFuns(ATerm t) */

static size_t calcUniqueAFuns(ATerm t)
{
  size_t nr_unique = 0;
  size_t  i, arity;
  AFun sym;
  ATermList list;

  if (IS_MARKED(t->header))
  {
    return 0;
  }

  switch (ATgetType(t))
  {
    case AT_INT:
      if (!at_lookup_table[AS_INT]->count++)
      {
        nr_unique = 1;
      }
      break;

    case AT_APPL:
      sym = ATgetAFun((ATermAppl) t);
      nr_unique = AT_isMarkedAFun(sym) ? 0 : 1;
      assert(at_lookup_table[sym]);
      at_lookup_table[sym]->count++;
      AT_markAFun(sym);
      arity = ATgetArity(sym);
      for (i = 0; i < arity; i++)
      {
        nr_unique += calcUniqueAFuns(ATgetArgument((ATermAppl)t, i));
      }
      break;

    case AT_LIST:
      list = (ATermList)t;
      while (!ATisEmpty(list) && !IS_MARKED(list->header))
      {
        SET_MARK(list->header);
        if (!at_lookup_table[AS_LIST]->count++)
        {
          nr_unique++;
        }
        nr_unique += calcUniqueAFuns(ATgetFirst(list));
        list = ATgetNext(list);
      }
      if (ATisEmpty(list) && !IS_MARKED(list->header))
      {
        SET_MARK(list->header);
        if (!at_lookup_table[AS_EMPTY_LIST]->count++)
        {
          nr_unique++;
        }
      }
      break;
  }

  SET_MARK(t->header);

  return nr_unique;
}

/*}}}  */
/**
 * Calculate the number of unique symbols
 */

/*{{{  size_t AT_calcUniqueAFuns(ATerm t) */

size_t AT_calcUniqueAFuns(ATerm t)
{
  size_t result = calcUniqueAFuns(t);
  AT_unmarkIfAllMarked(t);

  return result;
}

/*}}}  */

/*{{{  static int AT_compareArguments(ATermAppl t1, ATermAppl t2)  */

static int AT_compareArguments(ATermAppl t1, ATermAppl t2)
{
  size_t arity1;
  size_t arity2;
  size_t i;
  ATerm arg1;
  ATerm arg2;
  int result = 0;

  arity1 = ATgetArity(ATgetAFun(t1));
  arity2 = ATgetArity(ATgetAFun(t2));


  for (i = 0; result == 0 && i < arity1 && i < arity2; i++)
  {
    arg1 = ATgetArgument(t1, i);
    arg2 = ATgetArgument(t2, i);
    result = ATcompare(arg1,arg2);
  }

  if (arity1 < arity2)
  {
    return -1;
  }
  else if (arity1 > arity2)
  {
    return 1;
  }

  return result;

}

/*}}}  */
/*{{{  static int AT_compareAppls(ATermAppl t0, ATermAppl t2) */

static int AT_compareAppls(ATermAppl t1, ATermAppl t2)
{
  AFun fun1;
  AFun fun2;
  char* name1;
  char* name2;
  int result;

  fun1 = ATgetAFun(t1);
  fun2 = ATgetAFun(t2);

  name1 = ATgetName(fun1);
  name2 = ATgetName(fun2);

  result = strcmp(name1,name2);
  if (result != 0)
  {
    return result;
  }

  return AT_compareArguments(t1,t2);

}

/*}}}  */
/*{{{  static int AT_compareInts(ATermInt t1, ATermInt t2)  */

static int AT_compareInts(ATermInt t1, ATermInt t2)
{
  int i1;
  int i2;
  i1 = ATgetInt(t1);
  i2 = ATgetInt(t2);
  if (i1 < i2)
  {
    return -1;
  }
  else if (i1 > i2)
  {
    return 1;
  }
  return 0;
}

/*}}}  */
/*{{{  static int AT_compareLists(ATermList t1, ATermList t2)  */

static int AT_compareLists(ATermList t1, ATermList t2)
{
  size_t length1;
  size_t length2;
  ATerm elt1;
  ATerm elt2;
  int result = 0;

  while (result == 0 && !ATisEmpty(t1) && !ATisEmpty(t2))
  {
    elt1 = ATgetFirst(t1);
    elt2 = ATgetFirst(t2);

    result = ATcompare(elt1,elt2);

    t1 = ATgetNext(t1);
    t2 = ATgetNext(t2);
  }

  if (result != 0)
  {
    return result;
  }

  length1 = ATgetLength(t1);
  length2 = ATgetLength(t2);

  if (length1 < length2)
  {
    return -1;
  }
  else if (length1 > length2)
  {
    return 1;
  }
  return 0;
}

/*}}}  */
/*{{{  int ATcompare(ATerm t1, ATerm t2) */

int ATcompare(ATerm t1, ATerm t2)
{
  size_t type1;
  size_t type2;
  int result = 0;

  if (ATisEqual(t1, t2))
  {
    return 0;
  }

  type1 = ATgetType(t1);
  type2 = ATgetType(t2);

  if (type1 < type2)
  {
    return -1;
  }
  else if (type1 > type2)
  {
    return 1;
  }

  switch (type1)
  {
    case AT_APPL:
      result = AT_compareAppls((ATermAppl) t1, (ATermAppl) t2);
      break;
    case AT_INT:
      result = AT_compareInts((ATermInt) t1, (ATermInt) t2);
      break;
    case AT_LIST:
      result = AT_compareLists((ATermList) t1, (ATermList) t2);
      break;
    default:
      ATabort("Unknown ATerm type %d\n", type1);
      break;
  }

  return result;
}

/*}}}  */

} // namespace aterm
