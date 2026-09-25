#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>

#define _CRT_SECURE_NO_WARNINGS
#define ADDRESS_LENGTH 64  // 64-bit memory addressing

// Structs
typedef struct cacheline
{
    int valid;
    long long tag;
    int lru;
} CacheLine;

typedef struct cacheset
{ 
    CacheLine *lines;
} CacheSet;

/*
 * this function provides a standard way for your cache
 * simulator to display its final statistics (i.e., hit and miss)
 */
void print_summary(int hits, int misses, int evictions)
{
    printf("hits:%d misses:%d evictions:%d\n", hits, misses, evictions);
}

/*
 * print usage info
 */
void print_usage(char* argv[])
{
    printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n", argv[0]);
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag.\n");
    printf("  -s <num>   Number of set index bits.\n");
    printf("  -E <num>   Number of lines per set.\n");
    printf("  -b <num>   Number of block offset bits.\n");
    printf("  -t <file>  Trace file.\n");
    printf("\nExamples:\n");
    printf("  linux>  %s -s 4 -E 1 -b 4 -t traces/trace01.dat\n", argv[0]);
    printf("  linux>  %s -v -s 8 -E 2 -b 4 -t traces/trace01.dat\n", argv[0]);
    exit(0);
}

/*
 * Helper to get index of bits
 */
long long get_set_index(long long address, int b, int set_count)
{
    long long set_index = (address >> b) & (set_count - 1);
    return set_index;
}

/*
 * Helper to get tag from address
 */
long long get_tag(long long address, int b, int s)
{
    long long tag = address >> (b + s);
    return tag;
}

/*
* Function to update the lru in the cache
*/
void update_lru(CacheSet* cache, int set_index, int E, int used_line)
{
    for (int i = 0; i < E;  i++)
    {
        if(cache[set_index].lines[i].valid  == 1)
        {
            if (i == used_line)
            {
                cache[set_index].lines[i].lru = 0;
            }
            else
            {
                cache[set_index].lines[i].lru++;
            }
        }
    }
}

/*
* Helper to get access to cache
*/
int access_cache(CacheSet* cache, int E, int b, int s, long long address)
{
    int set_count = 1 << s;
    int set_index = get_set_index(address, b, set_count);
    long long tag = get_tag(address, b, s);
    int empty_line = -1;
    int lru_line = 0;


    for (int i = 0; i < E; i++)                                                                         
    {
        if (cache[set_index].lines[i].valid == 0 && empty_line == -1)
        {
            empty_line = i;
        }

        if (cache[set_index].lines[i].valid == 1 && cache[set_index].lines[i].tag == tag)
        {
            update_lru(cache, set_index, E, i);  
            return 1;                          
        }

        
    }

    if (empty_line != -1)
    {
        cache[set_index].lines[empty_line].valid = 1;
        cache[set_index].lines[empty_line].tag = tag;
        update_lru(cache, set_index, E, empty_line);

        return 0;
    }

    int largest_lru = cache[set_index].lines[0].lru;

    for (int i = 0; i < E; i++)
    {
        if (cache[set_index].lines[i].lru > largest_lru)
        {
            largest_lru = cache[set_index].lines[i].lru;
            lru_line = i;
        }
    }

    cache[set_index].lines[lru_line].tag = tag;
    update_lru(cache, set_index, E, lru_line);

    return 2;
}    

/*
 * starting point
 */
int main(int argc, char* argv[])
{
    // Options
    int s = 0;
    int E = 0;
    int b = 0;
    int c;

    char* trace_file = NULL;
    int verbose = 0;

    // Trace information
    char operation;
    long long address;
    int size;

    // Counts
    int hit_count = 0;
    int miss_count = 0;
    int eviction_count = 0;

    // Parse command-line arguments
    while ((c = getopt(argc, argv, "s:E:b:t:vh")) != -1)
    {
        switch (c)
        {
            case 's':
                s = atoi(optarg);
                break;

            case 'E':
                E = atoi(optarg);
                break;

            case 'b':
                b = atoi(optarg);
                break;

            case 't':
                trace_file = optarg;
                break;

            case 'v':
                verbose = 1;
                break;

            case 'h':
                print_usage(argv);
                exit(0);

            default:
                print_usage(argv);
                exit(1);
        }
    }

    // Validate command-line arguments
    if (s <= 0 || E <= 0 || b < 0 || trace_file == NULL)
    {
        print_usage(argv);
        exit(1);
    }

    // Cache stuff
    int set_count = 1 << s;

    CacheSet* cache = malloc(set_count * sizeof(CacheSet));

    if (cache == NULL)
    {
        printf("Error. . .\n");
        exit(1);
    }

    // Allocate and initialize cache lines
    for (int i = 0; i < set_count; i++)
    {
        cache[i].lines = malloc(E * sizeof(CacheLine));

        if (cache[i].lines == NULL)
        {
            printf("Error . . .\n");
            exit(1);
        }

        for (int j = 0; j < E; j++)
        {
            cache[i].lines[j].valid = 0;
            cache[i].lines[j].tag = 0;
            cache[i].lines[j].lru = 0;
        }
    }

    // Open trace file
    FILE* file = fopen(trace_file, "r");

    if (file == NULL)
    {
        printf("Error opening trace file.\n");
        exit(1);
    }

    // Read trace file
    while (fscanf(file, " %c %llx,%d", &operation, &address, &size) == 3)
    {
        if(operation == 'I')
        {
            continue;
        }

        if(operation == 'L' || operation == 'S')
        {
            int result = access_cache(cache, E, b, s, address);

            if(result == 1)
            {            
                hit_count++;
            }
            else if (result == 2)
            {
                miss_count++;
                eviction_count++;
            }
            else
            {
                miss_count++;
            }

            if(verbose)
            {
                printf("%c %llx,%d", operation, address, size);

                if(result == 1)
                {
                    printf(" hit\n");
                }
                else if(result == 2)
                {
                    printf(" miss eviction\n");
                }
                else
                {
                    printf(" miss\n");
                }
            }
        }

        if (operation == 'M')
        {
            int result1 = access_cache(cache, E, b, s, address);

            if (result1 == 1)
            {
                hit_count++;
            }
            else if (result1 == 2)
            {
                miss_count++;
                eviction_count++;
            }
            else
            {
                miss_count++;
            }

            int result2 = access_cache(cache, E, b, s, address);

            if (result2 == 1)
            {
                hit_count++;
            }
            else if (result2 == 2)
            {
                miss_count++;
                eviction_count++;
            }
            else
            {
                miss_count++;
            }

            if (verbose)
            {
                printf("%c %llx,%d", operation, address, size);

                if (result1 == 1)
                {
                    printf(" hit");
                }
                else if (result1 == 2)
                {
                    printf(" miss eviction");
                }
                else
                {
                    printf(" miss");
                }

                if (result2 == 1)
                {
                    printf(" hit\n");
                }
                else if (result2 == 2)
                {
                    printf(" miss eviction\n");
                }
                else
                {
                    printf(" miss\n");
                }
            }
        }
    }

    fclose(file);

    for (int i = 0; i < set_count; i++)
    {
        free(cache[i].lines);
    }

    free(cache);
    // output cache hit and miss statistics
    print_summary(hit_count, miss_count, eviction_count);

    // assignment done. life is good!
    return 0;
}   