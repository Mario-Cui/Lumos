#pragma once
#include "Core/Core.h"

namespace Lumos
{
    class Memory
    {
    public:
        static void* AlignedAlloc(size_t size, size_t alignment);
        static void AlignedFree(void* data);

        static void* NewFunc(std::size_t size, const char* file, int line);
        static void DeleteFunc(void* p);
    };

    struct Arena
    {
        uint64_t Position;
        uint64_t CommitPosition;
        uint64_t Align;
        uint64_t Size;
        Arena* Ptr;
        uint64_t _unused_[3];
    };

    struct ArenaTemp
    {
        Arena* arena;
        uint64_t pos;
    };

    int GetArenaCount();
    Arena* GetArena(int index);

    Arena* ArenaAlloc(uint64_t size);
    Arena* ArenaAllocDefault();
    void ArenaRelease(Arena* arena);
    void* ArenaPushNoZero(Arena* arena, uint64_t size);
    void* ArenaPushAligner(Arena* arena, uint64_t alignment);
    void* ArenaPush(Arena* arena, uint64_t size);
    void ArenaPopTo(Arena* arena, uint64_t pos);
    void ArenaSetAutoAlign(Arena* arena, uint64_t align);
    void ArenaPop(Arena* arena, uint64_t size);
    void ArenaClear(Arena* arena);
    uint64_t ArenaPos(Arena* arena);
    ArenaTemp ArenaTempBegin(Arena* arena);
    void ArenaTempEnd(ArenaTemp temp);

#define PushArrayNoZero(arena, type, count) (type*)ArenaPushNoZero((arena), sizeof(type) * (count))
#define PushArray(arena, type, count) (type*)ArenaPush((arena), sizeof(type) * (count))
#define PushObject(arena, type) PushArrayNoZero(arena, type, 1)

#define ArenaTempBlock(arena, name) \
    ArenaTemp name = { 0 };         \
    DeferLoop(name = ArenaTempBegin(arena), ArenaTempEnd(name))
}

#define CUSTOM_MEMORY_ALLOCATOR
#if defined(CUSTOM_MEMORY_ALLOCATOR) && defined(LUMOS_ENGINE)

#ifdef LUMOS_PLATFORM_WINDOWS
void* __CRTDECL operator new(size_t size);
void* __CRTDECL operator new[](size_t size);
void* __CRTDECL operator new(size_t size, const char* desc);
void* __CRTDECL operator new[](size_t size, const char* desc);
void* __CRTDECL operator new(size_t size, const char* file, int line);
void* __CRTDECL operator new[](size_t size, const char* file, int line);

void __CRTDECL operator delete(void* memory);
void __CRTDECL operator delete(void* memory, const char* desc);
void __CRTDECL operator delete(void* memory, const char* file, int line);
void __CRTDECL operator delete[](void* memory);
void __CRTDECL operator delete[](void* memory, const char* desc);
void __CRTDECL operator delete[](void* memory, const char* file, int line);
#else
void* operator new(size_t size);
void* operator new[](size_t size);
void* operator new(size_t size, const char* desc);
void* operator new[](size_t size, const char* desc);
void* operator new(size_t size, const char* file, int line);
void* operator new[](size_t size, const char* file, int line);

void operator delete(void* memory) noexcept;
void operator delete(void* memory, const char* desc) noexcept;
void operator delete(void* memory, const char* file, int line) noexcept;
void operator delete[](void* memory) noexcept;
void operator delete[](void* memory, const char* desc) noexcept;
void operator delete[](void* memory, const char* file, int line) noexcept;

#endif

#endif
