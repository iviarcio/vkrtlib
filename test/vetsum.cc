#include <iostream>
#include <chrono>
#include "../src/vkrtlib.h"

using namespace std;
using namespace chrono;
using namespace vkrtl;

#define N 512

int main()
{
    // Create a Vulkan Object
    Object obj(VKRTL_verbose);
    // Get the GPU device
    Device &device = obj.getDevice();

    // Create the shared buffers
    Buffer bufferA(device, sizeof(float) * N, true);
    Buffer bufferB(device, sizeof(float) * N, true);
    Buffer bufferC(device, sizeof(float) * N, true);

    // map the buffers A & B to CPU fill out
    float *A = (float *)bufferA.map();
    float *B = (float *)bufferB.map();
    for (int i = 0; i < N; i++){
        A[i] = (float)i;
        B[i] = (float)i;
        if (i < 15)
            cout << "A[" << i << "] = " << A[i] << endl;
    }
    bufferA.unmap();
    bufferB.unmap();

    Program prog(device, "../shaders/vetsum.spv");
    Kernel kn(device, prog, "vetsum", {STORAGE_BUFFER, STORAGE_BUFFER, STORAGE_BUFFER});
    Arguments args(kn, {bufferA, bufferB, bufferC});
    CommandBuffer cmd(device, kn, args);
    cmd.dispatch(N);
    cmd.barrier();
    cmd.end();

    // time to the execution on the GPU
    steady_clock::time_point start = steady_clock::now();
    device.submit(cmd);
    device.wait();
    cout << "Compute time = " << duration_cast<milliseconds>(steady_clock::now() - start).count()
         << "ms" << endl;

    // map the C buffer to CPU and print first results
    float *C = (float *)bufferC.map();
    for (int i = 0; i < N; i++){
        if (i < 15)
            cout << "C[" << i << "] = " << C[i] << endl;
    }
    bufferC.unmap();

    // Cleanup
    bufferA.destroy();
    bufferB.destroy();
    bufferC.destroy();
    cmd.destroy();
    args.destroy();
    kn.destroy();
    prog.destroy();
    device.destroy();

    return 0;
}
