__kernel void vet2sum(__global float *A,
                      __global float *B,
                      __global float *C,
                      __const int N)
{
    int i = get_global_id(0);
    if (i < N)
        C[i] = A[i] + B[i];
}
