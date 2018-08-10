__kernel void vet3sum(__global float *A,
                      __global float *B,
                      __global float *C)
{
    int b0 = get_group_id(0);
    int t0 = get_local_id(0);

    C[16 * b0 + t0] = (A[16 * b0 + t0] + B[16 * b0 + t0]);
}
