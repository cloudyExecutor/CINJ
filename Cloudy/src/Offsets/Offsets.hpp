#pragma once

#include <cstdint>

namespace Offsets {
    constexpr std::uint64_t ModuleFlags = 0x1b0 - 0x4;
    constexpr std::uint64_t IsCoreScript = 0x1b0;

    constexpr std::uint64_t Bytecode = 0x10;
    constexpr std::uint64_t BytecodeSize = 0x20;

    constexpr std::uint64_t ModuleScriptBytecode = 0x168;
    constexpr std::uint64_t LocalScriptBytecode = 0x1C0;

    constexpr std::uint64_t Size = 0x8;
    constexpr std::uint64_t Children = 0x70;
    constexpr std::uint64_t ChildrenEnd = 0x8;
    constexpr std::uint64_t Name = 0x68;
    constexpr std::uint64_t Parent = 0x50;
    constexpr std::uint64_t ClassName = 0x8;
    constexpr std::uint64_t ClassDescriptor = 0x18;

    constexpr std::uint64_t ValueBase = 0xC8;
    constexpr std::uint64_t LocalPlayer = 0x118;
        /*
        namespace player {
            constexpr std::uint64_t localplayer = 0x118;
            constexpr std::uint64_t userid = 0x1E8;
            constexpr std::uint64_t displayname = 0x100;
        };
        */
};