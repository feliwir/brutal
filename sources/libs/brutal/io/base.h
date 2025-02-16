#pragma once

#include <brutal/base/error.h>
#include <brutal/base/types.h>

#define IO_MAP_READ (1 << 0)
#define IO_MAP_WRITE (1 << 1)

typedef uint32_t IoMapHint;

typedef enum
{
    IO_WHENCE_START,
    IO_WHENCE_CURRENT,
    IO_WHENCE_END,
} IoWhence;

typedef struct
{
    IoWhence whence;
    SSize position;
} IoSeek;

IoSeek io_seek_from_start(SSize position);

IoSeek io_seek_from_current(SSize position);

IoSeek io_seek_from_end(SSize position);

typedef Result(Error, size_t) IoResult;
