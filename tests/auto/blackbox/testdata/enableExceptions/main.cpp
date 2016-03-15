#include <stdexcept>

int main() {
#ifdef FORCE_FAIL_VS
#error "Microsoft Visual C++ cannot disable exceptions at compile-time"
#endif
    throw std::runtime_error("failed");
}
