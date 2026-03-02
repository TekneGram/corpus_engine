#pragma once

#include <cstddef>
#include <fstream>
#include <stdexcept>
#include <string>

namespace teknegram {

    class BinaryWriter {
        public:
            explicit BinaryWriter(const std::string& path)
            : out_(path.c_str(), std::ios::binary | std::ios::out | std::ios::trunc) {
                if (!out_) {
                    throw std::runtime_error("Failed to open binary output files: " + path);
                }
            }

            template <typename T>
            void write(const T& value) {
                // Call <ofstream>'s write method to write one typed object using sizeof(T), e.g., uint32_t
                out_.write(reinterpret_cast<const char*>(&value), sizeof(T));
                if (!out_) {
                    throw std::runtime_error("Failed to write typed binary value.");
                }
            }

            void write_raw(const void* data, std::size_t size) {
                // Call <ofstream>'s write method to write an arbitrary memory block - we provide pointer + byte count, e.g., write a buffer/array/string bytes
                out_.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
                if (!out_) {
                    throw std::runtime_error("Failed to write binary payload.");
                }
            }

        private:
            std::ofstream out_; // out_ is of type <ofstream>
    };
}