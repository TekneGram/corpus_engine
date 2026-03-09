#pragma once

#include <string>

namespace teknegram {

class ProgressEmitter {
    public:
        virtual ~ProgressEmitter() {}
        virtual void emit(const std::string& message, int percent) const = 0;
};

class NullProgressEmitter : public ProgressEmitter {
    public:
        virtual void emit(const std::string& message, int percent) const;
};

} // namespace teknegram
