#ifndef _POSIX_STRINGS_H
#define _POSIX_STRINGS_H

#include "types.h"
#define bzero(ptr, size) (memset((ptr), '\0', (size)), (void) 0)

#define __OS          "q"  /* Operation Suffix */
#define __OP          "r"  /* Operand Prefix */
#define __FIXUP_ALIGN ".align 8"
#define __FIXUP_WORD  ".quad"

#ifndef BYTES_PER_LONG
#define BYTES_PER_LONG 8
#endif

int ffs (int i);
int ffsl (long int li);
int ffsll (long long int lli);

#endif /* _POSIX_STRINGS_H */
