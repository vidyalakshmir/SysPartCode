#include "../hello/hello.h"

int main() {
    void (*funcPtr)() = printHello;

    funcPtr();

    return 0;
}

