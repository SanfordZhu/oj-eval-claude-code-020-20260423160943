int main() { printf("Before init\\n"); fflush(stdout); void *p = malloc(128*1024*1024); printf("After malloc\\n"); fflush(stdout); return 0; }
