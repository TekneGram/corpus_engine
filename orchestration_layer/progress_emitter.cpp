#include "progress_emitter.hpp"

namespace teknegram {

void NullProgressEmitter::emit(const std::string& message, int percent) const {
    (void)message;
    (void)percent;
}

} // namespace teknegram
