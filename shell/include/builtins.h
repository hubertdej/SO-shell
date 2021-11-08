#ifndef _BUILTINS_H_
#define _BUILTINS_H_

#define BUILTIN_ERROR 2

typedef int (*builtin_ptr)(char**);

typedef struct {
  char* name;
  builtin_ptr fun;
} builtin_pair;

extern builtin_pair builtins_table[];

builtin_ptr getBuiltin(char* command_name);

#endif /* !_BUILTINS_H_ */
