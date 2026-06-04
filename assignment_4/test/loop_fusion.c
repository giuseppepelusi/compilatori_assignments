void test_fusible_constant(int *A, int *B) {
    for (int i = 0; i < 100; i++) {
        A[i] = i * 2;
    }
    for (int i = 0; i < 100; i++) {
        B[i] = i * 3;
    }
}

void test_different_trip_count(int *A, int *B, int N, int M) {
    for (int i = 0; i < N; i++) {
        A[i] = i;
    }
    for (int i = 0; i < M; i++) {
        B[i] = i * 2;
    }
}

void test_fusible_raw_same_iter(int *A, int *B) {
    for (int i = 0; i < 100; i++) {
        A[i] = i * 2;
    }
    for (int i = 0; i < 100; i++) {
        B[i] = A[i] + 1;
    }
}
