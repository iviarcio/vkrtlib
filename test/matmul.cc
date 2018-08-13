#include <iostream>
#include "../src/vkrtlib.h"

using namespace std;
using namespace vkrtl;

#define N 512

int main()
{
    // Create a Vulkan Object
    Object obj;
    // Get the GPU device
    Device &dev = obj.getDevice();

    // Create the shared buffers
    Buffer bufferC(dev, sizeof(float) * N * N, true);
    Buffer bufferA(dev, sizeof(float) * N * N, true);
    Buffer bufferB(dev, sizeof(float) * N * N, true);

    // map the bufferA & bufferB to CPU fill out
    float *A = (float *)bufferA.map();
    float *B = (float *)bufferB.map();
    for (int i = 0; i < N; i++){
        for (int j = 0; j < N; j++){
            A[i*N + j] = (float)(i + j);
            B[i*N + j] = (float)(i + j);
            if (i < 5 && j < 5)
                cout << "A[" << i << ", " << j << "] = " << A[i*N+j] << endl;
        }
    }
    bufferA.unmap();
    bufferB.unmap();

    // Create a buffer to pass the constant N
    Buffer bN(dev, sizeof(int32_t));
    int n = N;
    bN.offload(&n);

    Program prog(dev, "../shaders/matmul.spv");
    Kernel kn(dev, prog, "matmul", {STORAGE_BUFFER, STORAGE_BUFFER, STORAGE_BUFFER, STORAGE_BUFFER});
    Arguments args(kn, {bufferA, bufferB, bufferC, bN});
    CommandBuffer cmd(dev, kn, args);
    cmd.dispatch(N, N);
    cmd.barrier();
    cmd.end();

    dev.submit(cmd);
    dev.wait();

    // map the bufferC to CPU and print
    float *C = (float *)bufferC.map();
    for (int i = 0; i < N; i++){
        for (int j = 0; j < N; j++){
            if (i < 5 && j < 5)
                cout << "C[" << i << ", " << j << "] = " << C[i*N+j] << endl;
        }
    }
    bufferC.unmap();

    // Cleanup
    bufferA.destroy();
    bufferB.destroy();
    bufferC.destroy();
    bN.destroy();
    cmd.destroy();
    args.destroy();
    kn.destroy();
    prog.destroy();
    dev.destroy();

    return 0;
}
