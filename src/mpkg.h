#ifndef MPKG_H
#define MPKG_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

//MPG
#define MPKGMAGIC 0x0047504D
#define RES_LABEL_LENGTH 128

typedef struct {
    char *start_pointer;
    long length;
} Mpkg;

typedef struct {
    uint32_t magic;
    uint32_t item_count;
} MpkgHeader;

typedef struct {
    char path[RES_LABEL_LENGTH];
    long offset;
    long length;
} MpkgResLabel;

Mpkg OpenMpkg(const char *path);
char* FetchDataFromMpkg(Mpkg pkg, const char* path, long *length);
MpkgResLabel* FetchResLabels(Mpkg pkg, uint32_t *size);

#ifdef MPKG_IMPLEMENTATION

Mpkg OpenMpkg(const char *path)
{
    FILE *file = fopen(path, "rb");
    if(!file)
    {
        printf("Failed to open file %d\n", path);
        return (Mpkg){0};
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file);

    char *data = (char*)malloc(length);
    size_t read_result = fread(data, length, 1, file);
    if(!read_result)
    {
        printf("Failed to read file %s\n", path);
        fclose(file);
        return (Mpkg){0};
    }

    fclose(file);
    return (Mpkg) {
        .start_pointer = data,
        .length = length
    };
}

char* FetchDataFromMpkg(Mpkg pkg, const char* path, long *length)
{
    char *pointer = pkg.start_pointer;
    MpkgHeader *header = (MpkgHeader*)pointer;
    pointer += sizeof(MpkgHeader);

    uint32_t low = 0, high = header->item_count - 1;
    while(true)
    {
        uint32_t i = (low + high) / 2;

        MpkgResLabel *label = (MpkgResLabel*)(pointer + i * sizeof(MpkgResLabel));

        if(!strcmp(path, label->path))
        {
            *length = label->length;
            return pkg.start_pointer + label->offset;
        }
        else
        {
            if(low == high) break;
            if(high - low == 1 & i == low) {low = high;continue;}
            if(high - low == 1 & i == high) {high = low;continue;}

            if(strcmp(path, label->path) < 0)
            {
                high = i;
                continue;
            }
            else
            {
                low = i;
                continue;
            }
        }
    }

    printf("Cannot find %s\n", path);
    *length = 0;
    return NULL;
}

MpkgResLabel* FetchResLabels(Mpkg pkg, uint32_t *size)
{
    char *pointer = pkg.start_pointer;
    MpkgHeader *header = (MpkgHeader*)pointer;
    pointer += sizeof(MpkgHeader);

    *size = header->item_count;

    return (MpkgResLabel*) pointer;
}
#endif

#endif
