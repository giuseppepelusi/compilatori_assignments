int a = 10;
int b = 20;

int test_basic(int n) {
    int result = 0;
    int x = a + b;
    for (int i = 0; i < n; i++) {
        int inv = a * b;
        result += inv + i;
    }
    return result;
}

int test_chain(int n) {
    int result = 0;
    for (int i = 0; i < n; i++) {
        int t1 = a + b;
        int t2 = t1 * 2;
        result += t2 + i;
    }
    return result;
}

int test_not_invariant(int n) {
    int result = 0;
    for (int i = 0; i < n; i++) {
        int dep = i * 3;
        result += dep;
    }
    return result;
}

int test_nested(int n) {
    int result = 0;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            int inv = a * b;
            result += inv + i + j;
        }
    }
    return result;
}
