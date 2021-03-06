#include "filesys/free-map.h"
#include <bitmap.h>
#include <debug.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/synch.h"

static struct file *free_map_file;   /* Free map file. */
static struct bitmap *free_map;      /* Free map, one bit per sector. */
static struct lock map_lock;         /* Lock on the free map */
static bool init_complete;

/* Initializes the free map. */
void
free_map_init (void) 
{
  free_map = bitmap_create (block_size (fs_device));
  if (free_map == NULL)
    PANIC ("bitmap creation failed--file system device is too large");
  bitmap_mark (free_map, FREE_MAP_SECTOR);
  bitmap_mark (free_map, ROOT_DIR_SECTOR);
  lock_init(&map_lock);
  init_complete = true;
}

/* Allocates CNT consecutive sectors from the free map and stores
   the first into *SECTORP.
   Returns true if successful, false if not enough consecutive
   sectors were available or if the free_map file could not be
   written. */
bool
free_map_allocate (size_t cnt, block_sector_t *sectorp)
{
  ASSERT(cnt == 1);
  lock_acquire(&map_lock);
  block_sector_t sector = bitmap_scan_and_flip (free_map, 0, cnt, false);
  if (init_complete 
      && sector != BITMAP_ERROR
      && free_map_file != NULL
      && !bitmap_write (free_map, free_map_file))
    {
      bitmap_set_multiple (free_map, sector, cnt, false); 
      sector = BITMAP_ERROR;
    }
  if (sector != BITMAP_ERROR)
  {
    *sectorp = sector;
  }

  // When free_map is first being written to a file,
  // we need to use free_map allocate!

  lock_release(&map_lock);
  return sector != BITMAP_ERROR;
}

bool
free_map_flush (void)
{
  bool success = false;
  lock_acquire(&map_lock);
  if (free_map_file != NULL && bitmap_write(free_map, free_map_file))
  {
    success = true;
  }
  else
  {
    PANIC("Free_map_flush error");
  }
  lock_release(&map_lock);

  return success;
}

/* Makes CNT sectors starting at SECTOR available for use. */
void
free_map_release (block_sector_t sector, size_t cnt)
{
  lock_acquire(&map_lock);
  ASSERT (bitmap_all (free_map, sector, cnt));
  bitmap_set_multiple (free_map, sector, cnt, false);
  bitmap_write (free_map, free_map_file);
  lock_release(&map_lock);
}

/* Opens the free map file and reads it from disk. */
void
free_map_open (void) 
{
  //bitmap_dump (free_map);
  lock_acquire(&map_lock);
  free_map_file = file_open (inode_open (FREE_MAP_SECTOR));
  if (free_map_file == NULL)
    PANIC ("can't open free map");
  if (!bitmap_read (free_map, free_map_file))
    PANIC ("can't read free map");
  lock_release(&map_lock);
  //bitmap_dump (free_map);
}

/* Writes the free map to disk and closes the free map file. */
void
free_map_close (void) 
{
  init_complete = true;
  free_map_flush();
  file_close (free_map_file);
}

/* Creates a new free map file on disk and writes the free map to
   it. */
void
free_map_create (void) 
{
  init_complete = false;
  /* Create inode. */
  if (!inode_create (FREE_MAP_SECTOR, bitmap_file_size (free_map), false))
    PANIC ("free map creation failed");

  /* Write bitmap to file. */
  free_map_file = file_open (inode_open (FREE_MAP_SECTOR));
  if (free_map_file == NULL)
    PANIC ("can't open free map");
  if (!bitmap_write (free_map, free_map_file))
    PANIC ("can't write free map");
}
