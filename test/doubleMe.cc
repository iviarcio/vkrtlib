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
    // Create the shared buffer
    Buffer buffer(device, sizeof(float) * N, true);

    // map the buffer to CPU fill out
    float *A = (float *)buffer.map();
    for (int i = 0; i < N; i++){
        A[i] = (float)i;
        if (i < 15)
            cout << "A[" << i << "] = " << A[i] << endl;
    }
    buffer.unmap();

    Program prog(device, "../shaders/doubleMe.spv");
    Kernel kn(device, prog, "doubleMe", {STORAGE_BUFFER});
    Arguments args(kn, {buffer});
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

    // map the buffer to CPU and print
    float *B = (float *)buffer.map();
    for (int i = 0; i < N; i++){
        if (i < 15)
            cout << "B[" << i << "] = " << B[i] << endl;
    }
    buffer.unmap();

    // Cleanup
    buffer.destroy();
    cmd.destroy();
    args.destroy();
    kn.destroy();
    prog.destroy();
    device.destroy();

    return 0;
}
