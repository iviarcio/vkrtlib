__kernel void doubleMe(__global float *A)
{
    int i = get_global_id(0);
    A[i] *= 2;
}

__kernel void tripleMe(__global float *A)
{
    int i = get_global_id(0);
    A[i] *= 3;
}
