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
    Device &dev = obj.getDevice();

    // Create the shared buffer
    Buffer buffer(dev, sizeof(float) * N, true);
    // map the buffer to CPU fill out
    float *B = (float *)buffer.map();
    for (int i = 0; i < N; i++){
        B[i] = (float)i;
        if (i < 15)
            cout << "B[" << i << "] = " << B[i] << endl;
    }
    buffer.unmap();

    Program prog(dev, "../shaders/doubleMe.spv");
    CommandBuffer cmd(dev);

    cmd.begin(); // start recording command buffer

    Kernel kn1(dev, prog, "doubleMe", {STORAGE_BUFFER});
    kn1.bindTo(cmd);
    Arguments args1(kn1, {buffer});
    args1.bindTo(cmd);
    cmd.dispatch(N);
    cmd.barrier();

    Kernel kn2(dev, prog, "tripleMe", {STORAGE_BUFFER});
    kn2.bindTo(cmd);
    Arguments args2(kn2, {buffer});
    args2.bindTo(cmd);
    cmd.dispatch(N);
    cmd.barrier();

    cmd.end(); // end recording

    // submit the command buffer and measure the time to the execution on the GPU
    steady_clock::time_point start = steady_clock::now();
    dev.submit(cmd);
    dev.wait();
    cout << "Compute time = " << duration_cast<milliseconds>(steady_clock::now() - start).count()
         << "ms" << endl;

    // map the buffer to CPU and print
    B = (float *)buffer.map();
    for (int i = 0; i < N; i++){
        if (i < 15)
            cout << "B[" << i << "] = " << B[i] << endl;
    }
    buffer.unmap();

    // Cleanup
    buffer.destroy();
    cmd.destroy();
    args1.destroy();
    args2.destroy();
    kn1.destroy();
    kn2.destroy();
    prog.destroy();
    dev.destroy();

    return 0;
}
