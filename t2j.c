#include "t2j.h"

#define Assert(expression)                                                                                             \
    while (!(expression))                                                                                              \
    *(volatile int *)0 = 0

#define PushArray(arena, type, count) (type *)ArenaPush((arena), sizeof(type) * (count))
#define PushStruct(arena, type) (type *)ArenaPush((arena), sizeof(type))

// [CurrentState][NodeType] -> NextState (there is no transition from, only to, UNKNOWN)
static u8 StateTransitions[6][5] = {{6, 6, 1, 3, 6}, {2, 2, 1, 3, 6}, {2, 2, 1, 3, 6},
                                    {6, 6, 6, 6, 5}, {6, 6, 6, 6, 5}, {4, 4, 1, 3, 4}};

static void *ArenaPush(arena *Arena, u64 Size)
{
    // TODO: We can run OOM (we only allocate 1Gb atm), make more allocations when needed, e.g. through a linked list
    void *Memory = Arena->Memory;
    Arena->Offset += Size;
    Arena->Memory = (u8 *)Arena->Memory + Arena->Offset;
    return Memory;
}

static void SHA1Digest(context *Context, byte *Destination, byte *Input, u32 Length)
{
    u32 Size = Length < 64 ? 64 : Length;
    // Size must be a multiple of 64
    while (Size % 64)
    {
        Size += 64 - (Size % 64);
    }

    u8 *Message = PushArray(Context->Arena, u8, Size);

    // Copy the input
    for (u32 Index = 0; Index < Length; Index++)
    {
        Message[Index] = (u8)Input[Index];
    }

    // Trailing '1' bit
    Message[Length] = 0x80;

    // Trailing input length in bits
    u64 BitLength = (u64)(Length * 8);
    Message[Size - 8] = (u8)(BitLength >> 56 & 0xff);
    Message[Size - 7] = (u8)(BitLength >> 48 & 0xff);
    Message[Size - 6] = (u8)(BitLength >> 40 & 0xff);
    Message[Size - 5] = (u8)(BitLength >> 32 & 0xff);
    Message[Size - 4] = (u8)(BitLength >> 24 & 0xff);
    Message[Size - 3] = (u8)(BitLength >> 16 & 0xff);
    Message[Size - 2] = (u8)(BitLength >> 8 & 0xff);
    Message[Size - 1] = (u8)BitLength;

    u32 H0 = 0x67452301;
    u32 H1 = 0xEFCDAB89;
    u32 H2 = 0x98BADCFE;
    u32 H3 = 0x10325476;
    u32 H4 = 0xC3D2E1F0;
    u32 A, B, C, D, E, F, K, Temp;

    for (u32 Chunk = 0; Chunk < (Size / 64); Chunk++)
    {
        u32 Words[80];
        A = H0;
        B = H1;
        C = H2;
        D = H3;
        E = H4;

        for (u32 WordIndex = 0; WordIndex < 80; WordIndex++)
        {
            if (WordIndex < 16)
            {
                Temp = (Chunk * 64) + (WordIndex * 4);
                Words[WordIndex] = ((u32)Message[Temp] << 24) + ((u32)Message[Temp + 1] << 16) +
                                   ((u32)Message[Temp + 2] << 8) + (u32)Message[Temp + 3];
            }

            if (WordIndex >= 16)
            {
                Temp = Words[WordIndex - 3] ^ Words[WordIndex - 8] ^ Words[WordIndex - 14] ^ Words[WordIndex - 16];
                Words[WordIndex] = Temp << 1 | Temp >> 31;
            }

            if (WordIndex < 20)
            {
                F = (B & C) | ((~B) & D);
                K = 0x5A827999;
            }
            else if (WordIndex < 40)
            {
                F = B ^ C ^ D;
                K = 0x6ED9EBA1;
            }
            else if (WordIndex < 60)
            {
                F = (B & C) | (B & D) | (C & D);
                K = 0x8F1BBCDC;
            }
            else if (WordIndex < 80)
            {
                F = B ^ C ^ D;
                K = 0xCA62C1D6;
            }

            Temp = (A << 5 | A >> 27) + F + E + K + Words[WordIndex];
            E = D;
            D = C;
            C = B << 30 | B >> 2;
            B = A;
            A = Temp;
        }

        H0 += A;
        H1 += B;
        H2 += C;
        H3 += D;
        H4 += E;
    }

    snprintf(Destination, 41, "%08x%08x%08x%08x%08x", H0, H1, H2, H3, H4);
}

static int KeyEquals(string *Key, byte *A)
{
    byte *B = Key->Data;

    while (*A != 0)
    {
        if (*A++ != *B++)
        {
            return 0;
        }
    }

    return 1;
}

static void Bencode(byte *Destination, node *Node)
{
    node *Current = Node;
    u8 Process = 1;
    i32 BytesWritten = 0;

    while (Current)
    {
        if (Process)
        {
            switch (Current->Type)
            {
            case BENCODE_STR:
            case BENCODE_DICT_ENTRY:
                BytesWritten += sprintf(Destination + BytesWritten, "%d:", Current->String->Length);

                u32 Length = Current->String->Length;
                byte *Data = Current->String->Data;
                for (u32 Index = 0; Index < Length; Index++)
                {
                    Destination[BytesWritten++] = Data[Index];
                }

                break;
            case BENCODE_INT:
                BytesWritten += sprintf(Destination + BytesWritten, "i%lde", Current->Integer);
                break;
            case BENCODE_LIST:
                Destination[BytesWritten++] = 'l';
                break;
            case BENCODE_DICT:
                Destination[BytesWritten++] = 'd';
                break;
            }
        }
        else
        {
            if (Current->Type == BENCODE_LIST || Current->Type == BENCODE_DICT)
            {
                Destination[BytesWritten++] = 'e';
            }

            if (Current == Node)
            {
                break;
            }
        }

        if (Current->Head && Process)
        {
            Current = Current->Head;
            Process = 1;
        }
        else if (Current->Next)
        {
            Current = Current->Next;
            Process = 1;
        }
        else
        {
            Current = Current->Parent;
            Process = 0;
        }
    }
}

static void PrintNode(context *Context, node *Node)
{
    node *Current = Node;
    u8 Process = 1;

    while (Current)
    {
        if (Process)
        {
            switch (Current->Type)
            {
            case BENCODE_STR:
                if (Current->String->IsBinary)
                {
                    if (Context->Flags.PrintBinary)
                    {
                        if (Context->Flags.BinaryInHex)
                        {
                            printf("\"");
                            for (u32 StringIndex = 0; StringIndex < Current->String->Length; StringIndex++)
                            {
                                printf("0x%02hhX", Current->String->Data[StringIndex]);
                            }
                            printf("\"");
                        }
                        else
                        {
                            fwrite(Current->String->Data, sizeof(byte), Current->String->Length, stdout);
                        }
                    }
                    else
                    {
                        printf(Current->Next ? "\"[BLOB]\"," : "\"[BLOB]\"");
                    }
                }
                else
                {
                    printf(Current->Next ? "\"%.*s\"," : "\"%.*s\"", Current->String->Length, Current->String->Data);
                }
                break;
            case BENCODE_INT:
                printf(Current->Next ? "%ld," : "%ld", Current->Integer);
                break;
            case BENCODE_LIST:
                printf(Current->Head ? "[" : "[],");
                break;
            case BENCODE_DICT:
                printf(Current->Head ? "{" : "{},}");
                break;
            case BENCODE_DICT_ENTRY:
                printf("\"%.*s\":", Current->String->Length, Current->String->Data);
                break;
            }
        }
        else
        {
            // This is when we're going up the list
            if (Current->Type == BENCODE_LIST)
            {
                printf(Current->Next ? "]," : "]");
            }
            else if (Current->Type == BENCODE_DICT)
            {
                printf(Current->Next ? "}," : "}");
            }
            else if (Current->Type == BENCODE_DICT_ENTRY)
            {
                if (Context->Flags.PrintInfoHash && Current->Head->Type == BENCODE_DICT &&
                    KeyEquals(Current->String, "info"))
                {
                    u32 Length = Current->Head->ByteLength;
                    byte Hash[41];
                    byte *Buffer = PushArray(Context->Arena, byte, Length);
                    Bencode(Buffer, Current->Head);
                    SHA1Digest(Context, Hash, Buffer, Length);
                    printf(",\"info_hash\":\"%s\"", Hash);
                }

                if (Current->Next)
                {
                    printf(",");
                }
            }

            // NOTE: We've reached our original node
            if (Current == Node)
            {
                break;
            }
        }

        // Depth first
        if (Current->Head && Process)
        {
            Current = Current->Head;
            Process = 1;
        }
        else if (Current->Next)
        {
            Current = Current->Next;
            Process = 1;
        }
        else
        {
            Current = Current->Parent;
            Process = 0;
        }
    }
}

static string *ConsumeString(context *Context)
{
    string *String = PushStruct(Context->Arena, string);
    String->IsBinary = 0;
    String->Data = 0;
    String->Length = 0;

    byte Character = (byte)fgetc(Context->Stream);
    Context->BytesRead++;

    while (Character != EOF && Character != ':')
    {
        if (Character < '0' || Character > '9')
        {
            fprintf(stderr, "t2j: invalid string size !0-9\n");
            exit(1);
        }

        String->Length = (String->Length * 10) + (u32)(Character - '0');
        Character = (byte)fgetc(Context->Stream);
        Context->BytesRead++;
    }

    if (Character == EOF)
    {
        fprintf(stderr, "t2j: no string to consume after size\n");
        exit(1);
    }

    Assert(Character == ':');

    // Empty string
    if (String->Length == 0)
    {
        return String;
    }

    String->Data = PushArray(Context->Arena, byte, String->Length);

    // NOTE: we don't check for EOF since some binary data might be set to -1
    for (u32 Index = 0; Index < String->Length; Index++)
    {
        Character = (byte)fgetc(Context->Stream);
        String->Data[Index] = Character;
        Context->BytesRead++;

        if (!String->IsBinary && (Character < 32 || Character > 126))
        {
            String->IsBinary = 1;
        }
    }

    return String;
}

static i64 ConsumeInteger(context *Context)
{
    byte Character = (byte)fgetc(Context->Stream);
    Context->BytesRead++;

    if (Character == EOF || Character == 'e')
    {
        fprintf(stderr, "t2j: invalid integer, EOF\n");
        exit(1);
    }

    byte Peek = (byte)fgetc(Context->Stream);
    ungetc(Peek, Context->Stream);

    if (Character == '-' && Peek == '0')
    {
        fprintf(stderr, "t2j: invalid integer, i-0 (negative zero)\n");
        exit(1);
    }

    if (Character == '0' && Peek != 'e')
    {
        fprintf(stderr, "t2j: invalid integer, leading zero\n");
        exit(1);
    }

    u8 Signed = 0;

    if (Character == '-')
    {
        Signed = 1;
        Character = (byte)fgetc(Context->Stream);
        Context->BytesRead++;
    }

    i64 Integer = 0;

    while (Character != EOF && Character != 'e')
    {
        if (Character < '0' || Character > '9')
        {
            fprintf(stderr, "t2j: invalid integer !0-9\n");
            exit(1);
        }

        Integer = (Integer * 10) + (i64)(Character - '0');
        Character = (byte)fgetc(Context->Stream);
        Context->BytesRead++;
    }

    if (Character == EOF)
    {
        fprintf(stderr, "t2j: invalid integer, EOF\n");
        exit(1);
    }

    Assert(Character == 'e');
    return Signed ? -Integer : Integer;
}

static parse_result Parse(context *Context)
{
    enum parse_state NextState = PARSE_EMPTY;
    node *Next = {0};
    node *Current = {0};
    byte Character = (byte)fgetc(Context->Stream);
    Context->BytesRead++;

    while (Character != EOF)
    {
        // Consume the next bencoded value
        if (Character >= '0' && Character <= '9')
        {
            // Give back the stolen size character
            ungetc(Character, Context->Stream);
            Context->BytesRead--;

            Next = PushStruct(Context->Arena, node);
            Next->ByteLength = Context->BytesRead;

            if (NextState == PARSE_NEW_DICT || NextState == PARSE_APPEND_DICT)
            {
                Next->Type = BENCODE_DICT_ENTRY;
            }
            else
            {
                Next->Type = BENCODE_STR;
            }

            Next->String = ConsumeString(Context);
            Next->ByteLength = Context->BytesRead - Next->ByteLength;
        }
        else if (Character == 'i')
        {
            Next = PushStruct(Context->Arena, node);
            Next->Type = BENCODE_INT;
            Next->ByteLength = Context->BytesRead;
            Next->Integer = ConsumeInteger(Context);
            Next->ByteLength = Context->BytesRead - Next->ByteLength + 1;
        }
        else if (Character == 'l')
        {
            Next = PushStruct(Context->Arena, node);
            Next->Type = BENCODE_LIST;
            Next->ByteLength = Context->BytesRead;
        }
        else if (Character == 'd')
        {
            Next = PushStruct(Context->Arena, node);
            Next->Type = BENCODE_DICT;
            Next->ByteLength = Context->BytesRead;
        }
        else if (Character == 'e')
        {
            Current = Current->Parent;

            if (Current->Type == BENCODE_LIST || Current->Type == BENCODE_DICT)
            {
                Current->ByteLength = Context->BytesRead - Current->ByteLength + 1;
            }

            if (!Current->Parent)
            {
                parse_result Result = {Current, 0};
                return Result;
            }

            if (Current->Parent->Type == BENCODE_LIST)
            {
                NextState = PARSE_APPEND_LIST;
            }

            if (Current->Parent->Type == BENCODE_DICT_ENTRY)
            {
                // For dictionary entries we need to go up another step
                Current = Current->Parent;
                NextState = PARSE_APPEND_DICT;
            }

            // There is no next node when going upwards
            Next = 0;
        }
        else
        {
            parse_result Result = {0, "t2j: unknown leading character for bencoding"};
            return Result;
        }

        if (!Next)
        {
            Character = (byte)fgetc(Context->Stream);
            Context->BytesRead++;
            continue;
        }

        if (NextState == PARSE_EMPTY)
        {
            if (Next->Type == BENCODE_STR || Next->Type == BENCODE_INT)
            {
                parse_result Result = {Next, 0};
                return Result;
            }

            Current = Next;
        }
        else if (NextState == PARSE_NEW_LIST || NextState == PARSE_NEW_DICT)
        {
            Next->Parent = Current;
            Current->Head = Next;
            Current = Next;
        }
        else if (NextState == PARSE_APPEND_LIST || NextState == PARSE_APPEND_DICT)
        {
            Next->Parent = Current->Parent;
            Current->Next = Next;
            Current = Next;
        }
        else if (NextState == PARSE_ENTRY_VALUE)
        {
            Next->Parent = Current;
            Current->Head = Next;

            if (Next->Type == BENCODE_LIST || Next->Type == BENCODE_DICT)
            {
                Current = Current->Head;
            }
        }

        NextState = StateTransitions[NextState][Current->Type];
        Assert(NextState != PARSE_UNKNOWN);
        Character = (byte)fgetc(Context->Stream);
        Context->BytesRead++;
    }

    Current->ByteLength = Context->BytesRead;
    parse_result Result = {Current, 0};
    return Result;
}

parse_result Torrent2Json(context *Context)
{
    parse_result Result = Parse(Context);

    if (Result.Error)
    {
        return Result;
    }

    PrintNode(Context, Result.Value);
    printf("\n");
    return Result;
}

void PrintUsage(void)
{
    fprintf(stderr, "t2j v0.2.0\n");
    fprintf(stderr, "Sebastian Bengtegård, https://github.com/trumtomte/t2j\n\n");
    fprintf(stderr, "t2j decodes bencoded files (e.g. .torrent files) into JSON.\n\n");
    fprintf(stderr, "USAGE:\n");
    fprintf(stderr, "    t2j [OPTIONS] FILE\n");
    fprintf(stderr, "    t2j -h\n\n");
    fprintf(stderr, "OPTIONS:\n");
    fprintf(stderr, "    -b Print binary data (otherwise marked as [BLOB] in the output).\n");
    fprintf(stderr, "    -i Print the info (sha1) hash as part of the output.\n");
    fprintf(stderr, "    -x Print binary in hexadecimal as \"0x0A0x0B0x0C (etc.)\".\n");
    exit(1);
}
