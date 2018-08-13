__kernel void matmul(__global float *A,
                     __global float *B,
                     __global float *C,
                     __const int N)
{
    int j = get_global_id(0);
    int i = get_global_id(1);
    for (int k=0; k < N; ++k)
      C[i*N + j] = A[i*N + k] + B[k*N + j];
}
