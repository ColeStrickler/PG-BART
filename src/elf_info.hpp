#ifndef ELF_INFO_HPP
#define ELF_INFO_HPP

#include <elf.h>
#include <gelf.h>
#include <libelf.h>

#include <fcntl.h>
#include <unistd.h>

#include <vector>
#include <string>
#include <algorithm>
#include <iostream>
#include <cxxabi.h>

struct FunctionInfo {
    uint64_t start;
    uint64_t end;
    std::string name;
};

const FunctionInfo* findFunction(
    const std::vector<FunctionInfo>& functions,
    uint64_t addr);

std::vector<FunctionInfo> loadFunctions(const char* path);
std::string demangle(const char* name);
uint64_t compute_load_bias(const char* binary_path);
uint64_t get_elf_preferred_base(const char* path);
#endif