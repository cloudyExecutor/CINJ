#pragma once

#include <vector>
#include <string>

std::vector<char> Compress_Bytecode_Jest(const std::string& bytecode, size_t& byte_size);


std::string IsCompilable(const std::string& Source, bool ShouldReturn);


std::string Compile(const std::string& Source);