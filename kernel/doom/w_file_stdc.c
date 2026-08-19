//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	WAD I/O functions.
//

#include <stdio.h>

#include "m_misc.h"
#include "w_file.h"
#include "z_zone.h"

typedef struct
{
    wad_file_t wad;
    FILE *fstream;
} stdc_wad_file_t;

extern wad_file_class_t stdc_wad_file;

static wad_file_t *W_StdC_OpenFile(char *path)
{
    stdc_wad_file_t *result;
    FILE *fstream;

    fstream = fopen(path, "rb");

    if (fstream == NULL)
    {
        return NULL;
    }

    // Aloca a estrutura do handle com malloc padrão
    result = malloc(sizeof(stdc_wad_file_t));
    if (!result) {
        fclose(fstream);
        return NULL;
    }

    result->wad.file_class = &stdc_wad_file;

    // Se o arquivo estiver carregado no RAMDisk MIGFS, mapeia o buffer diretamente em memoria (Zero-Copy)
    if (fstream->mig_file && fstream->mig_file->data) {
        result->wad.mapped = (byte*)fstream->mig_file->data;
        result->wad.length = (unsigned int)fstream->mig_file->size;
    } else {
        result->wad.mapped = NULL;
        result->wad.length = M_FileLength(fstream);
    }
    result->fstream = fstream;

    return &result->wad;
}

static void W_StdC_CloseFile(wad_file_t *wad)
{
    stdc_wad_file_t *stdc_wad;

    stdc_wad = (stdc_wad_file_t *) wad;

    fclose(stdc_wad->fstream);
    free(stdc_wad);
}

// Read data from the specified position in the file into the 
// provided buffer.  Returns the number of bytes read.
size_t W_StdC_Read(wad_file_t *wad, unsigned int offset,
                   void *buffer, size_t buffer_len)
{
    stdc_wad_file_t *stdc_wad;
    size_t result;

    stdc_wad = (stdc_wad_file_t *) wad;

    if (wad->mapped != NULL)
    {
        if (offset + buffer_len > wad->length) {
            buffer_len = (offset < wad->length) ? (wad->length - offset) : 0;
        }
        if (buffer_len > 0) {
            memcpy(buffer, wad->mapped + offset, buffer_len);
        }
        return buffer_len;
    }

    // Jump to the specified position in the file.
    fseek(stdc_wad->fstream, offset, SEEK_SET);

    // Read into the buffer.
    result = fread(buffer, 1, buffer_len, stdc_wad->fstream);

    return result;
}


wad_file_class_t stdc_wad_file = 
{
    W_StdC_OpenFile,
    W_StdC_CloseFile,
    W_StdC_Read,
};


