#include <string.h>
#include <string>

#include "spsc_ben.h"
#include "mpsc_ben.h"

using namespace mybenchmark;

constexpr int verify_mpsc_producer_num = 10;
constexpr int verify_mpsc_pre_producer_num = 100000;

int main(int argc, char** args) {

    if(argc < 2) return -1;

    if(strcmp(args[1], "1") == 0) {
        spsc::spsc_bench<std::string> spsc;

    } else if(strcmp(args[1], "2") == 0) {
        mpsc::mpsc_ben<std::string> mpsc;

    } else if(strcmp(args[1], "3") == 0) {
        mpsc::verify_mpsc verify;
        verify.verify(verify_mpsc_producer_num, verify_mpsc_pre_producer_num);
    }

    return 0;
}