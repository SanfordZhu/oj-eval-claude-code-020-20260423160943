int main() { void *p = malloc(128*1024*1024); init_page(p, 128*1024/4); void *r = alloc_pages(1); printf("Expected: %p, Got: %p\\n", p, r); return 0; }
