/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "kernel/hash.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "string.h"

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void vm_init(void)
{
	vm_anon_init();
	vm_file_init();
#ifdef EFILESYS /* For project 4 */
	pagecache_init();
#endif
	register_inspect_intr();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type(struct page *page)
{
	int ty = VM_TYPE(page->operations->type);
	switch (ty)
	{
	case VM_UNINIT:
		return VM_TYPE(page->uninit.type);
	default:
		return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim(void);
static bool vm_do_claim_page(struct page *page);
static struct frame *vm_evict_frame(void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable,
									vm_initializer *init, void *aux)
{

	ASSERT(VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page(spt, upage) == NULL)
	{
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page *page = malloc(sizeof(struct page));
		if (page == NULL) goto err;
		// printf("page type = %d", type);
		// printf("logical addr = %p\n", upage);
		if (VM_TYPE(type) == VM_ANON)
			uninit_new(page, upage, init, type, aux, anon_initializer);
		if (VM_TYPE(type) == VM_FILE)
			uninit_new(page, upage, init, type, aux, file_backed_initializer);
		page->writable = writable;
		page->is_loaded = false;
		/* TODO: Insert the page into the spt. */
		if (spt_insert_page(spt, page) == false)
		{
			free(page);
			//uncheckec : err발생시 aux 처리에 대한 검증 필요.
			goto err;
		}
		// printf("allocated vm address is : %p\n", spt_find_page(spt, upage)->va);
		return true;
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *spt_find_page(struct supplemental_page_table *spt, void *va)
{
	struct page *page;
	struct hash_elem *found_elem;
	page = (struct page *)malloc(sizeof(struct page));
	page->va = va;

	found_elem = hash_find(&spt->spt_hash, &page->hash_elem);
	if (found_elem != NULL)
	{
		return hash_entry(found_elem, struct page, hash_elem);
	}
	free(page);
	return NULL;
}
/* Insert PAGE into spt with validation. */
bool spt_insert_page(struct supplemental_page_table *spt UNUSED,
					 struct page *page UNUSED)
{
	int succ = hash_insert(&spt->spt_hash, &page->hash_elem) == NULL;
	return succ;
}

void spt_remove_page(struct supplemental_page_table *spt, struct page *page)
{
	vm_dealloc_page(page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim(void)
{
	struct frame *victim = NULL;
	/* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame(void)
{
	struct frame *victim UNUSED = vm_get_victim();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame(void)
{
	struct frame *frame = NULL;
	/* TODO: Fill this function. */

	void *addr = palloc_get_page(PAL_USER || PAL_ZERO);
	if (addr == NULL)
		PANIC("todo");

	frame = calloc(sizeof(struct frame), 1);

	frame->kva = addr;

	ASSERT(frame != NULL);
	ASSERT(frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth(void *addr UNUSED)
{
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp(struct page *page UNUSED)
{
}

/* Return true on success */
bool vm_try_handle_fault(struct intr_frame *f UNUSED, void *addr UNUSED,
						 bool user UNUSED, bool write UNUSED, bool not_present UNUSED)
{
	struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	void *page_addr = pg_round_down(addr);
	// printf("*page_fault_handler\n");
	// printf("claim address : %p\n", addr);
	// printf("*claim page address %p\n", page_addr);
	// printf("*not_present is : %d \n", not_present);

	if (not_present)
	{
		page = spt_find_page(spt, page_addr);
		if (page != NULL)
		{
			return vm_do_claim_page(page);
		}
	}
	return false;
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void vm_dealloc_page(struct page *page)
{
	destroy(page);
	free(page);
}

/* Claim the page that allocate on VA. */
bool vm_claim_page(void *va UNUSED)
{
	struct page *page = NULL;
	/* TODO: Fill this function */
	page = spt_find_page(&thread_current()->spt, va);
	if (page == NULL)
		return false;
	return vm_do_claim_page(page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page(struct page *page)
{
	struct frame *frame = vm_get_frame();

	/* Set links */
	frame->page = page;
	page->frame = frame;
	page->is_loaded = true;
	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	struct thread *cur_t = thread_current();
	pml4_set_page(cur_t->pml4, page->va, frame->kva, page->writable);

	return swap_in(page, frame->kva);
}

/* Initialize new supplemental page table */
void supplemental_page_table_init(struct supplemental_page_table *spt UNUSED)
{
	hash_init(&spt->spt_hash, spt_hash_func, page_table_entry_less_function, NULL);
}

/* Copy supplemental page table from src to dst */
bool supplemental_page_table_copy(struct supplemental_page_table *dst UNUSED,
                                  struct supplemental_page_table *src UNUSED) {
  struct hash_iterator iterator;
  struct page *parent_page, *child_page = NULL;
  struct page_info_transmitter *src_aux, *dst_aux = NULL;
  enum vm_type page_type;
  bool succ = false;

  hash_first(&iterator, &src->spt_hash);
  
  while (hash_next(&iterator)) {
    parent_page = hash_entry(hash_cur(&iterator), struct page, hash_elem);
	uint8_t parent_type = parent_page->operations->type;
    if (VM_TYPE(parent_type) == VM_UNINIT){
        src_aux = (struct page_info_transmitter *)parent_page->uninit.aux;
        dst_aux = (struct page_info_transmitter *)calloc(
            1, sizeof(struct page_info_transmitter));
        if (!dst_aux) return false;
        dst_aux->file = src_aux->file;
        dst_aux->read_bytes = src_aux->read_bytes;
        dst_aux->zero_bytes = src_aux->zero_bytes;
        dst_aux->ofs = src_aux->ofs;

        if (!vm_alloc_page_with_initializer(
                parent_page->uninit.type, parent_page->va, parent_page->writable,
                parent_page->uninit.init, dst_aux)) {

          free(dst_aux);
          return false;
        }
        continue;
	}
    if (!vm_alloc_page(VM_ANON | VM_MARKER_0, parent_page->va, parent_page->writable)) return false;
    if (!vm_claim_page(parent_page->va)) return false;
    child_page = spt_find_page(dst, parent_page->va);
    if (!child_page) return false;
    memcpy(child_page->frame->kva, parent_page->frame->kva, PGSIZE);
    // printf("DIE 4 !! \n");make
    /* TODO : copy-on-write 구현한다면 부모의 kva를 자식의 va가 가르키도록 설정 */
    }
	succ = true;
  	return succ;
  }

/* Free the resource hold by the supplemental page table */
void supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED)
{
	hash_clear(&spt->spt_hash, spt_destory);
}

bool page_table_entry_less_function(struct hash_elem *a, struct hash_elem *b, void *aux UNUSED)
{
	struct page *page_a = hash_entry(a, struct page, hash_elem);
	struct page *page_b = hash_entry(b, struct page, hash_elem);
	return page_a->va < page_b->va;
}
