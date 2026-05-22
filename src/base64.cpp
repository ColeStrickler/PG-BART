#include "base64.hpp"




std::string encodeBase64(uint64_t n) {
    // URL-safe alphabet (avoids + and /)
    const std::string chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789-_";

    if (n == 0)
        return std::string(1, chars[0]);

    std::string out;

    while (n > 0) {
        out += chars[n % 64];
        n /= 64;
    }

    std::reverse(out.begin(), out.end());
    return out;
}

uint64_t decodeBase64(const std::string& s) {
    const std::string chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789-_";

    uint64_t result = 0;

    for (char c : s) {
        size_t value = chars.find(c);
        if (value == std::string::npos)
            assert(false && "decodeBase64() invalid character");

        result = result * 64 + value;
    }

    return result;
}
