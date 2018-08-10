__kernel void comp(__global float *A,
                   __const int N)
{
    int i = get_global_id(0);
    if (i < N)
        A[i] = A[i] + 3;
}
