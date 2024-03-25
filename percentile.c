#include <stdio.h>
#include <stdlib.h>

int cmp_func(const void * a, const void * b) {
    unsigned long long int da = (*(unsigned long long int*)a);
    unsigned long long int db = (*(unsigned long long int*)b);

    return da - db;
}

int main(int argc, char **argv) {
    if(argc != 3) {
        fprintf(stderr, "Usage: %s <percentile> <file_name>\n", argv[0]);
        exit(-1);
    }

    FILE *fp = fopen(argv[2], "r");
    if(!fp) {
        exit(-1);
    }

    int n;
    int __attribute__((unused)) ret = fscanf(fp, "%d\n", &n);

    unsigned long long int *arr = (unsigned long long int*) malloc(n * sizeof(unsigned long long int));
    if(!arr) {
        exit(-1);
    }

    unsigned long long int val;
    for(int i = 0; i < n; i++) {
        ret = fscanf(fp, "%llu\n", &val);
        arr[i] = val;
    }

    double p = strtod(argv[1], NULL);
    int percentile = (p/100.0) * n;

    qsort(arr, n, sizeof(unsigned long long int), cmp_func);
    printf("%llu\n", arr[percentile]);

    free(arr);

    return 0;
}
