#ifndef B64_HPP
#define B64_HPP
#include <string>
#include <stdint.h>
#include <algorithm>
#include <cassert>

std::string encodeBase64(uint64_t n);
uint64_t decodeBase64(const std::string& s);







#endif