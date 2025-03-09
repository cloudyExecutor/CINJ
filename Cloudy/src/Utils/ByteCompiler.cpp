#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <vector>
#include "../../dependencies/Zstd/zstd.h"
#include <windows.h>
#include "../../dependencies/xxhash.h"
#include "../../dependencies/Luau/Compiler.h"
#include "../../dependencies/Luau/BytecodeBuilder.h"
#include "../../dependencies/Luau/BytecodeUtils.h"
#include "../../EntryPoint.hpp"
std::vector<char> Compress_Bytecode_Jest(const std::string& bytecode, size_t& byte_size) {
    const auto data_size = bytecode.size();
    const auto max_size = ZSTD_compressBound(data_size);
    auto buffer = std::vector<char>(max_size + 8);

    buffer[0] = 'R'; buffer[1] = 'S'; buffer[2] = 'B'; buffer[3] = '1';
    std::memcpy(&buffer[4], &data_size, sizeof(data_size));

    const auto compressed_size = ZSTD_compress(&buffer[8], max_size, bytecode.data(), data_size, ZSTD_maxCLevel());
    const auto size = compressed_size + 8;
    const auto key = XXH32(buffer.data(), size, 42u);
    const auto bytes = reinterpret_cast<const uint8_t*>(&key);

    for (auto i = 0u; i < size; ++i) {
        buffer[i] ^= bytes[i % 4] + i * 41u;
    }

    byte_size = size;
    buffer.resize(size);

    return buffer;
}


class BytecodeEncoder : public Luau::BytecodeEncoder {
    inline void encode(uint32_t* Dataaa, size_t Counntt) override {
        for (auto i = 0u; i < Counntt;) {
            auto& opcode = *reinterpret_cast<uint8_t*>(Dataaa + i);
            i += Luau::getOpLength(LuauOpcode(opcode));
            opcode *= 227;
        }
    }
};

Luau::CompileOptions CompileOptions;

std::string Compile(const std::string& Source)
{
    static BytecodeEncoder encoder = BytecodeEncoder();
    const std::string bytecode = Luau::compile(Source, {}, {}, &encoder);
    if (bytecode[0] == '\0') {
        std::string bytecodeP = bytecode;
        bytecodeP.erase(std::remove(bytecodeP.begin(), bytecodeP.end(), '\0'), bytecodeP.end());
    }
    return bytecode;
}


std::string IsCompilable(const std::string& Source, bool ShouldReturn) {
    static BytecodeEncoder encoder = BytecodeEncoder();
    std::string bytecode = Luau::compile(Source, {}, {}, &encoder);
    if (bytecode[0] == '\0') {
        bytecode.erase(std::remove(bytecode.begin(), bytecode.end(), '\0'), bytecode.end());
        return bytecode;
    }

    if (ShouldReturn) {
        return bytecode;
    }
    return "True";
}
