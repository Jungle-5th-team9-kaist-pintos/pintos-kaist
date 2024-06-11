/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "bitmap.h"
#include "devices/disk.h"
#include "vm/vm.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in(struct page *page, void *kva);
static bool anon_swap_out(struct page *page);
static void anon_destroy(struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
    .swap_in = anon_swap_in,
    .swap_out = anon_swap_out,
    .destroy = anon_destroy,
    .type = VM_ANON,
};

static struct bitmap *swap_bitmap;
static struct lock swap_lock;

/* Initialize the data for anonymous pages */
void vm_anon_init(void) {
  /* TODO: Set up the swap_disk. */
  swap_disk = disk_get(1, 1);
  // printf("dist size : %d\n", disk_size(swap_disk));
  swap_bitmap = bitmap_create(disk_size(swap_disk) / 8);
  lock_init(&swap_lock);
}

/* Initialize the file mapping */
bool anon_initializer(struct page *page, enum vm_type type, void *kva) {
  struct anon_page *anon_page = &page->anon;
  /* Set up the handler */

  page->operations = &anon_ops;
  page->type = VM_ANON;

  anon_page->swap_bitmap_idx = -1;

  return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool anon_swap_in(struct page *page, void *kva) {
  struct anon_page *anon_page = &page->anon;

  lock_acquire(&swap_lock);

  printf("swap_in call !! kva : %p\n", kva);
  for (size_t i = 0; i < 8; i++) {
    disk_read(swap_disk, anon_page->swap_bitmap_idx * 8 + i,
              kva + (DISK_SECTOR_SIZE * i));
  }
  printf("---- before flip\n");
  bitmap_dump(swap_bitmap);

  bitmap_flip(swap_bitmap, anon_page->swap_bitmap_idx);

  printf("---- after flip\n");
  bitmap_dump(swap_bitmap);

  lock_release(&swap_lock);

  return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool anon_swap_out(struct page *page) {
  struct anon_page *anon_page = &page->anon;
  size_t bitmap_idx;

  // printf("anon swap out() call !! \n");

  lock_acquire(&swap_lock);

  bitmap_idx = bitmap_scan_and_flip(swap_bitmap, 0, 1, false);
  if (bitmap_idx == BITMAP_ERROR || bitmap_idx == -1)
    PANIC("{anon_swap_out()} BITMAP ERROR !!\n");

  anon_page->swap_bitmap_idx = bitmap_idx;
  // printf("bitmap idx : %d\n", bitmap_idx);
  // printf("swap out kva : %p\n", page->frame->kva);
  for (size_t i = 0; i < 8; i++) {
    // printf("disk write ing...\n");
    disk_write(swap_disk, bitmap_idx * 8 + i,
               page->frame->kva + (DISK_SECTOR_SIZE * i));
  }
  // printf("disk write DONE !!\n");

  page->frame = NULL;

  lock_release(&swap_lock);

  return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void anon_destroy(struct page *page) {
  struct anon_page *anon_page = &page->anon;
}
