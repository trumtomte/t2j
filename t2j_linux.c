#include "t2j.h"
#define ARENA_SIZE 1024 * 1024 * 1024

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        PrintUsage();
	return 0;
    }

    arena Arena = {0};
    Arena.Memory = mmap(0, ARENA_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    context Context = {0};
    Context.Arena = &Arena;
    
    byte *Filename = 0;
    for (u16 ArgIndex = 1; ArgIndex < argc; ArgIndex++)
    {
        byte *Arg = (byte *)argv[ArgIndex];

        if (Arg[0] == '-' && *++Arg != '\0')
        {
            // TODO: Allow combining flags
            switch (*Arg)
            {
            case 'b':
                Context.Flags.PrintBinary = 1;
                break;
            case 'x':
                Context.Flags.BinaryInHex = 1;
                break;
            case 'i':
                Context.Flags.PrintInfoHash = 1;
                break;
            case 'h':
                PrintUsage();
		return 0;
                break;
            default:
                fprintf(stderr, "t2j: illegal option -%c\n", *Arg);
                PrintUsage();
		return 0;
            }
        }
        else
        {
            Filename = Arg;
        }
    }

    if (!Filename)
    {
        // TODO: read from stdin, if empty, then PrintUsage
        PrintUsage();
	return 0;
    }

    Context.Stream = fopen(Filename, "r");

    if (!Context.Stream)
    {
        fprintf(stderr, "t2j: unable to read file %s\n", Filename);
        exit(1);
    }

    parse_result Result = Torrent2Json(&Context);

    if (Result.Error)
    {
	fprintf(stderr, "%s\n", Result.Error);
        exit(1);
    }
    
    return 0;
}
