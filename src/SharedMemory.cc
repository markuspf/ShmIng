/*
 * SharedMemory: Shared Memory Parallelism in GAP
 */

extern "C" {
#include "src/compiled.h"          /* GAP headers */
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <time.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
}

#include <cerrno>

static Obj SHAREDMEMORY_Region_Type;

struct ShmRegion {
    Obj    type;
    int    fd;
    size_t size;
    char   *data;
};

typedef struct ShmRegion * ShmRegionPtr;
typedef const struct ShmRegion * ConstShmRegionPtr;

static ShmRegionPtr SHAREDMEMORY_REGION(Obj region) {
    return (ShmRegionPtr)ADDR_OBJ(region);
}

static ConstShmRegionPtr CONST_SHAREDMEMORY_REGION(Obj region) {
    return (ConstShmRegionPtr)CONST_ADDR_OBJ(region);
}

Obj FuncSHARED_MEMORY_SHMOPEN(Obj self, Obj name, Obj oflag)
{
    RequireStringRep("SHARED_MEMORY_SHMOPEN", name);
    RequireSmallInt("SHARED_MEMORY_SHMOPEN", oflag, "oflag");

    int moflag = INT_INTOBJ(oflag);

    // TODO: allow mode to be passed?
    int fd = shm_open(CSTR_STRING(name), moflag, 0600);

    return INTOBJ_INT(fd);
}

Obj FuncSHARED_MEMORY_SHMUNLINK(Obj self, Obj name)
{
    RequireStringRep("SHARED_MEMORY_SHMOPEN", name);

    // TODO: allow mode to be passed?
    int fd = shm_unlink(CSTR_STRING(name));

    return 0;
}

Obj FuncSHARED_MEMORY_FTRUNCATE(Obj self, Obj ifd, Obj isize)
{
    RequireSmallInt("SHARED_MEMORY_SHMOPEN", ifd, "ifd");
    RequireSmallInt("SHARED_MEMORY_SHMOPEN", isize, "isize");

    int fd = INT_INTOBJ(ifd);
    size_t size = INT_INTOBJ(isize);

    // TODO: Error handling
    ftruncate(fd, size);

    return 0;
}

Obj FuncSHARED_MEMORY_MMAP(Obj self, Obj ifd, Obj isize)
{
    RequireSmallInt("MMAP_SHARED_MEMORY", ifd, "fd");
    RequireSmallInt("MMAP_SHARED_MEMORY", isize, "size");

    size_t sz = INT_INTOBJ(isize);
    int fd = INT_INTOBJ(ifd);
    int flags = MAP_SHARED;

    if (fd == -1 || fd == 0) {
       flags |= MAP_ANONYMOUS;
    }

    Obj result = NewBag(T_DATOBJ, sizeof(struct ShmRegion));
    SetTypeDatObj(result, SHAREDMEMORY_Region_Type);

    SHAREDMEMORY_REGION(result)->size = sz;
    SHAREDMEMORY_REGION(result)->fd = fd;
    SHAREDMEMORY_REGION(result)->data = (char *)mmap(
        NULL, sz, PROT_READ | PROT_WRITE, flags, fd, 0);
    CHANGED_BAG(result);

    return result;
}

template <typename T>
Obj FuncSHARED_MEMORY_PEEK(Obj self, Obj region, Obj pos)
{
    RequireSmallInt("SHARED_MEMORY_PEEK", pos, "pos");

    UInt8 upos = UInt8_ObjInt(pos);
    T val = *((T *)(&(SHAREDMEMORY_REGION(region)->data[upos])));

    return ObjInt_UInt8((UInt8)(val));
}

template <typename T> 
Obj FuncSHARED_MEMORY_POKE(Obj self, Obj region, Obj pos, Obj val)
{
    RequireSmallInt("SHARED_MEMORY_POKE", pos, "pos");

    UInt8 upos = UInt8_ObjInt(pos);
    T uval = (T)(UInt8_ObjInt(val));

    *(T *)(&(SHAREDMEMORY_REGION(region)->data[upos])) = (T)(uval);

    return 0;
}

Obj FuncSHARED_MEMORY_PEEK_STRING(Obj self, Obj region, Obj pos, Obj len)
{
    RequireSmallInt("SHARED_MEMORY_PEEK", pos, "pos");
    RequireSmallInt("SHARED_MEMORY_PEEK_STRING", len, "len");

    char *mem = SHAREDMEMORY_REGION(region)->data;
    UInt8 upos = UInt8_ObjInt(pos);
    UInt8 ulen = UInt8_ObjInt(len);

    return MakeStringWithLen(&mem[upos], ulen);
}

Obj FuncSHARED_MEMORY_POKE_STRING(Obj self, Obj region, Obj pos, Obj val)
{
    RequireSmallInt("SHARED_MEMORY_POKE", pos, "pos");
    RequireStringRep("SHARED_MEMORY_POKE", val);

    char *mem = SHAREDMEMORY_REGION(region)->data;
    UInt8 upos = UInt8_ObjInt(pos);
    UInt8 ulen = GET_LEN_STRING(val);

    memcpy(&mem[upos], CHARS_STRING(val), ulen);

    return 0;
}

Obj FuncSHARED_MEMORY_SEMINIT(Obj self, Obj region, Obj pos, Obj val)
{
    RequireSmallInt("SHARED_MEMORY_SEMINIT", pos, "pos");
    RequireSmallInt("SHARED_MEMORY_SEMINIT", val, "val");

    char *mem = SHAREDMEMORY_REGION(region)->data;
    UInt8 upos = UInt8_ObjInt(pos);
    unsigned int uval = (unsigned int)UInt_ObjInt(val);

    return ObjInt_Int(sem_init((sem_t *)&mem[upos], 1, uval));
}

Obj FuncSHARED_MEMORY_SEMPOST(Obj self, Obj region, Obj pos)
{
    RequireSmallInt("SHARED_MEMORY_SEMPOST", pos, "pos");

    char *mem = SHAREDMEMORY_REGION(region)->data;
    UInt8 upos = UInt8_ObjInt(pos);

    return ObjInt_Int(sem_post((sem_t *)&mem[upos]));
}

Obj FuncSHARED_MEMORY_SEMWAIT(Obj self, Obj region, Obj pos)
{
    RequireSmallInt("SHARED_MEMORY_SEMWAIT", pos, "pos");

    char *mem = SHAREDMEMORY_REGION(region)->data;
    UInt8 upos = UInt8_ObjInt(pos);

    return ObjInt_Int(sem_wait((sem_t *)&mem[upos]));
}

Obj FuncSHARED_MEMORY_SEMTRYWAIT(Obj self, Obj region, Obj pos)
{
    RequireSmallInt("SHARED_MEMORY_SEMTRYWAIT", pos, "pos");

    char *mem = SHAREDMEMORY_REGION(region)->data;
    UInt8 upos = UInt8_ObjInt(pos);

    return ObjInt_Int(sem_trywait((sem_t *)&mem[upos]));
}

Obj FuncSHARED_MEMORY_SEMTIMEDWAIT(Obj self, Obj region, Obj pos, Obj timeout)
{
    RequireSmallInt("SHARED_MEMORY_SEMTRYWAIT", pos, "pos");

    char *mem = SHAREDMEMORY_REGION(region)->data;
    UInt8 upos = UInt8_ObjInt(pos);
    UInt8 utimeout = UInt8_ObjInt(timeout);

    struct timespec ts;

    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        return ObjInt_Int(-1);
    }
    // microseconds should be enough for everyone
    ts.tv_sec += utimeout / 1000000;
    ts.tv_nsec += (utimeout % 1000000) * 1000;

    return ObjInt_Int(sem_timedwait((sem_t *)&mem[upos], &ts));
}

// Table of functions to export
static StructGVarFunc GVarFuncs[] = {
    GVAR_FUNC(SHARED_MEMORY_SHMOPEN, 2, "name, oflags"),
    GVAR_FUNC(SHARED_MEMORY_SHMUNLINK, 1, "name"),
    GVAR_FUNC(SHARED_MEMORY_FTRUNCATE, 2, "fd, size"),
    GVAR_FUNC(SHARED_MEMORY_MMAP, 2, "size, fd"),
    GVAR_FUNC(SHARED_MEMORY_PEEK<Char>, 2, "shm, pos"),
    GVAR_FUNC(SHARED_MEMORY_PEEK<UInt2>, 2, "shm, pos"),
    GVAR_FUNC(SHARED_MEMORY_PEEK<UInt4>, 2, "shm, pos"),
    GVAR_FUNC(SHARED_MEMORY_PEEK<UInt8>, 2, "shm, pos"),
    GVAR_FUNC(SHARED_MEMORY_POKE<Char>, 3, "shm, pos, val"),
    GVAR_FUNC(SHARED_MEMORY_POKE<UInt2>, 3, "shm, pos, val"),
    GVAR_FUNC(SHARED_MEMORY_POKE<UInt4>, 3, "shm, pos, val"),
    GVAR_FUNC(SHARED_MEMORY_POKE<UInt8>, 3, "shm, pos, val"),
    GVAR_FUNC(SHARED_MEMORY_PEEK_STRING, 3, "shm, pos, length"),
    GVAR_FUNC(SHARED_MEMORY_POKE_STRING, 3, "shm, pos, string"),
    GVAR_FUNC(SHARED_MEMORY_SEMINIT, 3, "shm, pos, val"),
    GVAR_FUNC(SHARED_MEMORY_SEMPOST, 2, "shm, pos"),
    GVAR_FUNC(SHARED_MEMORY_SEMWAIT, 2, "shm, pos"),
    GVAR_FUNC(SHARED_MEMORY_SEMTRYWAIT, 2, "shm, pos"),
    GVAR_FUNC(SHARED_MEMORY_SEMTIMEDWAIT, 3, "shm, pos, timeout"),

    { 0 } /* Finish with an empty entry */
};

/****************************************************************************
**
*F  InitKernel( <module> ) . . . . . . . .  initialise kernel data structures
*/
static Int InitKernel( StructInitInfo *module )
{
    ImportGVarFromLibrary("SHAREDMEMORY_Region_Type", &SHAREDMEMORY_Region_Type);

    /* init filters and functions */
    InitHdlrFuncsFromTable( GVarFuncs );

    /* return success */
    return 0;
}

/****************************************************************************
**
*F  InitLibrary( <module> ) . . . . . . .  initialise library data structures
*/
static Int InitLibrary( StructInitInfo *module )
{
    /* init filters and functions */
    InitGVarFuncsFromTable( GVarFuncs );

    /* return success */
    return 0;
}

/****************************************************************************
**
*F  Init__Dynamic() . . . . . . . . . . . . . . . . . table of init functions
*/
static StructInitInfo module = {
    .type = MODULE_DYNAMIC,
    .name = "SharedMemory",
    .initKernel = InitKernel,
    .initLibrary = InitLibrary,
};

extern "C"
StructInitInfo *Init__Dynamic( void )
{
    return &module;
}
