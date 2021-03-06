#include <stdc.h>
#include <physmem.h>
#include <pmap.h>
#include <vm_object.h>
#include <vm_pager.h>

static vm_page_t *dummy_pager_fault(vm_object_t *obj, off_t offset) {
  return NULL;
}

static vm_page_t *anon_pager_fault(vm_object_t *obj, off_t offset) {
  assert(obj != NULL);

  vm_page_t *new_pg = pm_alloc(1);
  pmap_zero_page(new_pg);
  vm_object_add_page(obj, offset, new_pg);
  return new_pg;
}

vm_pager_t pagers[] = {
    [VM_DUMMY] = {.pgr_fault = dummy_pager_fault},
    [VM_ANONYMOUS] = {.pgr_fault = anon_pager_fault},
};
