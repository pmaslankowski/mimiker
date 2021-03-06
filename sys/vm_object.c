#define KL_LOG KL_VM
#include <klog.h>
#include <pool.h>
#include <pmap.h>
#include <physmem.h>
#include <vm_object.h>

static POOL_DEFINE(P_VMOBJ, "vm_object", sizeof(vm_object_t));

static inline int vm_page_cmp(vm_page_t *a, vm_page_t *b) {
  if (a->offset < b->offset)
    return -1;
  return a->offset - b->offset;
}

RB_PROTOTYPE_STATIC(pg_tree, vm_page, obj.tree, vm_page_cmp);
RB_GENERATE(pg_tree, vm_page, obj.tree, vm_page_cmp);

vm_object_t *vm_object_alloc(vm_pgr_type_t type) {
  vm_object_t *obj = pool_alloc(P_VMOBJ, PF_ZERO);
  TAILQ_INIT(&obj->list);
  RB_INIT(&obj->tree);
  obj->pager = &pagers[type];
  return obj;
}

void vm_object_free(vm_object_t *obj) {
  while (!TAILQ_EMPTY(&obj->list)) {
    vm_page_t *pg = TAILQ_FIRST(&obj->list);
    TAILQ_REMOVE(&obj->list, pg, obj.list);
    pm_free(pg);
  }
  pool_free(P_VMOBJ, obj);
}

vm_page_t *vm_object_find_page(vm_object_t *obj, off_t offset) {
  vm_page_t find = {.offset = offset};
  return RB_FIND(pg_tree, &obj->tree, &find);
}

bool vm_object_add_page(vm_object_t *obj, off_t offset, vm_page_t *page) {
  assert(is_aligned(page->offset, PAGESIZE));
  /* For simplicity of implementation let's insert pages of size 1 only */
  assert(page->size == 1);

  page->object = obj;
  page->offset = offset;

  if (!RB_INSERT(pg_tree, &obj->tree, page)) {
    obj->npages++;
    vm_page_t *next = RB_NEXT(pg_tree, &obj->tree, page);
    if (next)
      TAILQ_INSERT_BEFORE(next, page, obj.list);
    else
      TAILQ_INSERT_TAIL(&obj->list, page, obj.list);
    return true;
  }

  return false;
}

void vm_object_remove_page(vm_object_t *obj, vm_page_t *page) {
  page->offset = 0;
  page->object = NULL;

  TAILQ_REMOVE(&obj->list, page, obj.list);
  RB_REMOVE(pg_tree, &obj->tree, page);
  pm_free(page);
  obj->npages--;
}

vm_object_t *vm_object_clone(vm_object_t *obj) {
  vm_object_t *new_obj = vm_object_alloc(VM_DUMMY);
  new_obj->pager = obj->pager;

  vm_page_t *pg;
  TAILQ_FOREACH (pg, &obj->list, obj.list) {
    vm_page_t *new_pg = pm_alloc(1);
    pmap_copy_page(pg, new_pg);
    vm_object_add_page(new_obj, pg->offset, new_pg);
  }

  return new_obj;
}

void vm_map_object_dump(vm_object_t *obj) {
  vm_page_t *it;
  RB_FOREACH (it, pg_tree, &obj->tree)
    klog("(vm-obj) offset: 0x%08lx, size: %ld", it->offset, it->size);
}
