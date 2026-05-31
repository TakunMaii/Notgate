#ifndef MPKG_RAYLIBG_H
#define MPKG_RAYLIBG_H

#include "mpkg.h"
#include <stdlib.h>

Music MpkgLoadMusicStream(Mpkg mpkg, const char *type, const char* path)
{
    long size;
    char *data = FetchDataFromMpkg(mpkg, path, &size);

    return LoadMusicStreamFromMemory(type, data, (int)size);
}

Shader MpkgLoadShader(Mpkg mpkg, const char* vspath, const char* fspath)
{
    char *vscode = NULL;
    if(vspath)
    {
        long vssize;
        char *vsdata = FetchDataFromMpkg(mpkg, vspath, &vssize);
        vscode = (char*)malloc(vssize + 1);
    
        memcpy(vscode, vsdata, vssize);
        vscode[vssize] = 0;
    }

    char *fscode = NULL;
    if(fspath)
    {
        long fssize;
        char *fsdata = FetchDataFromMpkg(mpkg, fspath, &fssize);
        fscode = (char*)malloc(fssize + 1);
    
        memcpy(fscode, fsdata, fssize);
        fscode[fssize] = 0;
    }

    Shader shader = LoadShaderFromMemory(vscode, fscode);

    if(vscode)free(vscode);
    if(fscode)free(fscode);

    return shader;
}

Texture2D MpkgLoadTexture(Mpkg mpkg, const char* type, const char* path)
{
    long size;
    char *data = FetchDataFromMpkg(mpkg, path, &size);

    Image image = LoadImageFromMemory(type, data, (int)size);

    return LoadTextureFromImage(image);
}

Sound MpkgLoadSound(Mpkg mpkg, const char* type, const char* path)
{
    long size;
    char *data = FetchDataFromMpkg(mpkg, path, &size);

    Wave wave = LoadWaveFromMemory(type, data, (int)size);
    return LoadSoundFromWave(wave);
}

#endif
