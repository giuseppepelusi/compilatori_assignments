// FONDE: adiacenti, stesso trip count costante, nessuna dipendenza (array distinti).
void test_fusible_constant(int *A, int *B)
{
    for (int i = 0; i < 100; i++)
    {
        A[i] = i * 2;
    }
    for (int i = 0; i < 100; i++)
    {
        B[i] = i * 3;
    }
}

// NON FONDE: trip count diversi (N != M) - condizione 2.
void test_different_trip_count(int *A, int *B, int N, int M)
{
    for (int i = 0; i < N; i++)
    {
        A[i] = i;
    }
    for (int i = 0; i < M; i++)
    {
        B[i] = i * 2;
    }
}

// FONDE: dipendenza RAW su A a distanza 0 (B[i] usa A[i] della stessa iterazione).
void test_fusible_raw_same_iter(int *A, int *B)
{
    for (int i = 0; i < 100; i++)
    {
        A[i] = i * 2;
    }
    for (int i = 0; i < 100; i++)
    {
        B[i] = A[i] + 1;
    }
}

// NON FONDE: dipendenza a distanza negativa, B[i] usa A[i+3] di un'iterazione futura - condizione 4.
void test_negative_distance(int *A, int *B)
{
    for (int i = 0; i < 100; i++)
    {
        A[i] = i;
    }
    for (int i = 0; i < 100; i++)
    {
        B[i] = A[i + 3];
    }
}

// FONDE: limite simbolico, quindi loop guarded - esercita il ramo guarded di isAdjacent.
void test_guarded_same_trip_count(int *A, int *B, int N)
{
    for (int i = 0; i < N; i++)
    {
        A[i] = i * 2;
    }
    for (int i = 0; i < N; i++)
    {
        B[i] = i * 3;
    }
}
