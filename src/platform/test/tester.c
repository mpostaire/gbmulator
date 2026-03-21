#include "gbatester.h"
#include "gbtester.h"

int main(int argc, char **argv) {
    int ret  = gbtester_main(argc, argv);
    ret     |= gbatester_main(argc, argv);
    return ret;
}
