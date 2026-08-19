// Test per il pass di Loop-Invariant Code Motion (loop-icm-pass).
// Il commento sopra ogni funzione dice cosa viene spostato fuori dal loop e perche'.

int a = 10;
int b = 20;

// HOISTA 3: a*b non dipende da i, esce dal loop (load a, load b, mul).
int test_basic(int n) {
    int result = 0;
    int x = a + b;
    for (int i = 0; i < n; i++) {
        int inv = a * b;
        result += inv + i;
    }
    return result;
}

// HOISTA 4: catena di invarianti, t2 esce perche' t1 e' gia' stato marcato tale.
int test_chain(int n) {
    int result = 0;
    for (int i = 0; i < n; i++) {
        int t1 = a + b;
        int t2 = t1 * 2;
        result += t2 + i;
    }
    return result;
}

// HOISTA 0: i*3 dipende dalla variabile d'induzione, non e' invariante.
int test_not_invariant(int n) {
    int result = 0;
    for (int i = 0; i < n; i++) {
        int dep = i * 3;
        result += dep;
    }
    return result;
}

// HOISTA 7: a*b sale prima nel preheader interno, poi in quello esterno.
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

// NON hoista: il loop scrive 'a', quindi il load di 'a' non e' invariante.
int test_global_written(int n) {
    int result = 0;
    for (int i = 0; i < n; i++) {
        result += a;
        a = i;
    }
    return result;
}

// NON hoista: *p puo' aliasare 'a', il load non e' garantito invariante.
int test_aliasing_store(int *p, int n) {
    int result = 0;
    for (int i = 0; i < n; i++) {
        result += a;
        *p = i;
    }
    return result;
}
