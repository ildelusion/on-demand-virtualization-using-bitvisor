#ifndef TYPES_H
#define TYPES_H

#ifndef NULL
#define NULL ((void*)0)
#endif

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned int u32;
typedef signed int s32;
typedef unsigned long u64;
typedef signed long s64;

typedef unsigned char uint8_t;
typedef signed char int8_t;

typedef unsigned short uint16_t;
typedef signed short int16_t;

typedef unsigned int uint32_t;
typedef signed int int32_t;


typedef unsigned long uint64_t;
typedef signed long int64_t;

typedef unsigned long       uintptr_t;
typedef long                intptr_t;

typedef unsigned long		size_t;
#define INT_MAX         0x7fffffff
#define INT_MIN         (-INT_MAX-1)
#define UINT_MAX        0xffffffff



//typedef unsigned long long uint64_t;
//typedef signed long long int64_t;


#endif
