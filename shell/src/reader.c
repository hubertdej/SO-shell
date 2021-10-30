#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"

#define BUFFER_LENGTH (2 * MAX_LINE_LENGTH - 1)

// Returns a pointer to a string containing a line from standard input.
// The return value from getLine() points to static data which may be overwritten by subsequent calls.
// Returns NULL on EOF.
// When a line exceeding the limit is read, the function returns NULL and sets the errno flag to EIO.
// Since getLine() can return NULL on both success and failure, one should set errno to 0 before the call,
// and then determine if an error occurred by checking whether errno has a nonzero value after the call.
char* getLine() {
  static_assert(BUFFER_LENGTH >= 2 * MAX_LINE_LENGTH - 1, "reader buffer too small");

  static char buffer[BUFFER_LENGTH];
  static char* start_ptr = buffer;
  static char* end_ptr = buffer;
  static char* search_ptr = buffer;

  static bool discard = false;
  static bool eof_detected = false;

  if (eof_detected) {
    return NULL;
  }

  while (true) {
    if (search_ptr == end_ptr) {
      if ((end_ptr - buffer) + MAX_LINE_LENGTH > BUFFER_LENGTH) {
        memmove(buffer, start_ptr, end_ptr - start_ptr);
        ptrdiff_t shift = start_ptr - buffer;
        start_ptr -= shift;
        search_ptr -= shift;
        end_ptr -= shift;
      }
      ssize_t size = read(STDIN_FILENO, end_ptr, MAX_LINE_LENGTH);
      if (size == -1) {
        perror("read() failed");
        exit(EXIT_FAILURE);
      }
      if (size == 0) {
        eof_detected = true;
        *end_ptr = '\0';
        return (start_ptr != end_ptr) ? start_ptr : NULL;
      }
      end_ptr += size;
    }

    if (discard) {
      char* newline = memchr(search_ptr, '\n', end_ptr - search_ptr);
      if (newline != NULL) {
        start_ptr = search_ptr = newline + 1;
        discard = false;
      } else {
        // Since we are discarding all the read characters, we can reset the pointers.
        start_ptr = search_ptr = end_ptr = buffer;
      }
      continue;
    }

    int search_length = MAX_LINE_LENGTH - (search_ptr - start_ptr);
    bool search_until_limit = true;
    if (search_length > end_ptr - search_ptr) {
      search_length = end_ptr - search_ptr;
      search_until_limit = false;
    }

    char* newline = memchr(search_ptr, '\n', search_length);
    if (newline != NULL) {
      *newline = '\0';
      char* result = start_ptr;
      start_ptr = search_ptr = newline + 1;
      return result;
    }
    search_ptr += search_length;
    if (search_until_limit) {
      // MAX_LINE_LENGTH characters were searched, but the newline was not found.
      start_ptr = search_ptr;
      discard = true;
      errno = EIO;
      return NULL;
    }
  }
}
