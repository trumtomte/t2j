#ifndef T2J_H
#define T2J_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

typedef char byte;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t i32;
typedef int64_t i64;

typedef struct arena arena;
typedef struct context context;

struct arena
{
    void *Memory;
    u64 Offset;
};

struct context
{
    arena *Arena;
    FILE *Stream;
    u32 BytesRead;

    struct
    {
        u8 PrintBinary : 1;
        u8 BinaryInHex : 1;
        u8 PrintInfoHash : 1;
    } Flags;

    // TODO: More flags:
    // -f only output FIELD, e.g. 'info.files[0].name'?
};

void Torrent2Json(context *Context);
void PrintUsage(void);

#endif
