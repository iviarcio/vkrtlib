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

    // Create the shared buffer
    Buffer buffer(dev, sizeof(float) * N, true);
    // map the buffer to CPU fill out
    float *A = (float *)buffer.map();
    for (int i = 0; i < N; i++){
        A[i] = (float)i;
        if (i < 15)
            cout << "A[" << i << "] = " << A[i] << endl;
    }
    buffer.unmap();

    // Create a buffer to pass the constant N
    Buffer bN(dev, sizeof(int32_t));
    int n = N;
    bN.offload(&n);

    Program prog(dev, "../shaders/test.spv");
    Kernel kn(dev, prog, "comp", {STORAGE_BUFFER, STORAGE_BUFFER});
    Arguments args(kn, {buffer, bN});
    CommandBuffer cmd(dev, kn, args);
    cmd.dispatch(N);
    cmd.barrier();
    cmd.end();

    dev.submit(cmd);
    dev.wait();

    // map the buffer to CPU and print
    float *B = (float *)buffer.map();
    for (int i = 0; i < N; i++){
        if (i < 15)
            cout << "B[" << i << "] = " << B[i] << endl;
    }
    buffer.unmap();

    // Cleanup
    buffer.destroy();
    bN.destroy();
    cmd.destroy();
    args.destroy();
    kn.destroy();
    prog.destroy();
    dev.destroy();

    return 0;
}
