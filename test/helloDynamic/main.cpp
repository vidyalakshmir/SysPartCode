// helloDynamic/main.cpp
#include <iostream>
#include <dlfcn.h>

int main() {
    void* handle = dlopen("../hello/libhello.so", RTLD_LAZY);
    if (!handle) {
        std::cerr << "Failed to open library: " << dlerror() << std::endl;
        return 1;
    }

    dlerror();  // Clear any existing errors

    void (*printHello)() = (void (*)())dlsym(handle, "printHello");
    const char* dlsym_error = dlerror();
    if (dlsym_error) {
        std::cerr << "Failed to load symbol printHello: " << dlsym_error << std::endl;
        dlclose(handle);
        return 1;
    }

    printHello();

    dlclose(handle);
    return 0;
}

