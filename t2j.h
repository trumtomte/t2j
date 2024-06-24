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
typedef struct node node;
typedef struct string string;
typedef struct parse_result parse_result;

enum bencode_type
{
    BENCODE_STR,
    BENCODE_INT,
    BENCODE_LIST,
    BENCODE_DICT,
    BENCODE_DICT_ENTRY
};

enum parse_state
{
    PARSE_EMPTY,
    PARSE_NEW_LIST,
    PARSE_APPEND_LIST,
    PARSE_NEW_DICT,
    PARSE_APPEND_DICT,
    PARSE_ENTRY_VALUE,
    PARSE_UNKNOWN
};

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

struct string
{
    u8 IsBinary;
    byte *Data;
    u32 Length;
};

struct node
{
    enum bencode_type Type;
    node *Next;
    node *Head;
    node *Parent;
    string *String;
    i64 Integer;
    u32 ByteLength;
};

struct parse_result
{
    node *Value;
    byte *Error;
};

parse_result Torrent2JSON(context *Context);
void PrintUsage(void);

#endif
