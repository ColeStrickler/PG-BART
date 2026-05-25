#include "elf_info.hpp"
#include <cstdlib>
#include <cstring>
#include <elf.h>
#include <gelf.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>

std::string demangle(const char* name)
{
    int status = 0;

    char* result =
        abi::__cxa_demangle(name, nullptr, nullptr, &status);

    std::string out =
        (status == 0 && result) ? result : name;

    std::free(result);

    return out;
}

std::vector<FunctionInfo> loadFunctions(const char* path)
{
    std::vector<FunctionInfo> functions;

    if (elf_version(EV_CURRENT) == EV_NONE)
        throw std::runtime_error("ELF init failed");

    int fd = open(path, O_RDONLY);
    if (fd < 0)
        throw std::runtime_error("Could not open file");

    Elf* elf = elf_begin(fd, ELF_C_READ, nullptr);
    if (!elf)
        throw std::runtime_error("elf_begin failed");

    Elf_Scn* scn = nullptr;

    while ((scn = elf_nextscn(elf, scn)))
    {
        GElf_Shdr shdr;
        gelf_getshdr(scn, &shdr);

        if (shdr.sh_type != SHT_SYMTAB)
            continue;

        Elf_Data* data = elf_getdata(scn, nullptr);

        int count = shdr.sh_size / shdr.sh_entsize;

        for (int i = 0; i < count; i++)
        {
            GElf_Sym sym;

            if (!gelf_getsym(data, i, &sym))
                continue;

            if (ELF64_ST_TYPE(sym.st_info) != STT_FUNC)
                continue;

            if (sym.st_size == 0)
                continue;

            const char* name =
                elf_strptr(elf, shdr.sh_link, sym.st_name);

            if (!name)
                continue;

            functions.push_back({
                sym.st_value,
                sym.st_value + sym.st_size,
                demangle(name)
            });
        }
    }

    elf_end(elf);
    close(fd);

    std::sort(
        functions.begin(),
        functions.end(),
        [](auto& a, auto& b)
        {
            return a.start < b.start;
        });

    return functions;
}


/*
    Binary Search over a FunctionInfo vector
*/
const FunctionInfo* findFunction(
    const std::vector<FunctionInfo>& functions,
    uint64_t addr)
{
    auto it = std::upper_bound(
        functions.begin(),
        functions.end(),
        addr,
        [](uint64_t value, const FunctionInfo& f)
        {
            return value < f.start;
        });

    if (it == functions.begin())
        return nullptr;


    --it;

    if (addr >= it->start && addr < it->end)
        return &(*it);

    return nullptr;
}


uint64_t compute_load_bias(const char* binary_path)
{
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) {
        perror("fopen /proc/self/maps");
        return 0;
    }

    char resolved_binary[PATH_MAX] = {};
    const char* binary_match_path = realpath(binary_path, resolved_binary)
        ? resolved_binary
        : binary_path;

    char line[512];
    uint64_t bias = 0;

    while (fgets(line, sizeof(line), f))
    {
        char perms[5] = {};
        char mapped_path[PATH_MAX] = {};
        uint64_t start_addr = 0;
        uint64_t end_addr = 0;
        uint64_t offset = 0;
        char dev[32] = {};
        uint64_t inode = 0;

        int fields = sscanf(
            line,
            "%lx-%lx %4s %lx %31s %lu %4095s",
            &start_addr,
            &end_addr,
            perms,
            &offset,
            dev,
            &inode,
            mapped_path);

        if (fields == 7 &&
            std::strchr(perms, 'x') &&
            std::strcmp(mapped_path, binary_match_path) == 0)
        {
            uint64_t preferred_base = get_elf_preferred_base(binary_path);

            bias = start_addr - preferred_base;
            printf("[Load Bias] Detected: 0x%llx (runtime=0x%llx, preferred=0x%llx)\n",
                   static_cast<unsigned long long>(bias),
                   static_cast<unsigned long long>(start_addr),
                   static_cast<unsigned long long>(preferred_base));
            break;
        }
    }

    fclose(f);
    return bias;
}

uint64_t get_elf_preferred_base(const char* path)
{
    if (elf_version(EV_CURRENT) == EV_NONE)
        return 0;

    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;

    Elf* elf = elf_begin(fd, ELF_C_READ, nullptr);
    if (!elf) {
        close(fd);
        return 0;
    }

    uint64_t base = 0;

    size_t phnum;
    elf_getphdrnum(elf, &phnum);

    for (size_t i = 0; i < phnum; ++i)
    {
        GElf_Phdr ph;
        gelf_getphdr(elf, i, &ph);

        if (ph.p_type == PT_LOAD && (ph.p_flags & PF_X))  // First executable LOAD segment
        {
            base = ph.p_vaddr;
            break;
        }
    }

    elf_end(elf);
    close(fd);
    return base;
}
