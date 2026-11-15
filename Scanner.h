// Scanner.h - Heuristic ELF (Linux binary) virus scanner. Header-only. Never executes any file.
#ifndef LARPOS_SCANNER_H
#define LARPOS_SCANNER_H

#include <dirent.h>
#include <elf.h>
#include <sys/stat.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <string>
#include <vector>
#include "Hashmap.h"

namespace scan {

struct Finding { int points; std::string why; };

struct FileReport {
    std::string path, sha256;
    unsigned long long size = 0;
    int score = 0;
    std::vector<Finding> findings;
    void add(int pts, const std::string& why) { findings.push_back({pts, why}); score += pts; }
};

enum Level { CLEAN, SUSPICIOUS, DANGEROUS };
inline Level levelOf(int score) { return score >= 50 ? DANGEROUS : (score >= 20 ? SUSPICIOUS : CLEAN); }
inline const char* levelName(Level l) { return l == DANGEROUS ? "DANGEROUS" : (l == SUSPICIOUS ? "SUSPICIOUS" : "CLEAN"); }

// ------------------------------------------------------------ SHA-256
inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

inline std::string sha256Hex(const uint8_t* data, size_t len) {
    static const uint32_t K[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    uint32_t h[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};

    std::vector<uint8_t> msg(data, data + len);
    msg.push_back(0x80);
    while (msg.size() % 64 != 56) msg.push_back(0);
    uint64_t bits = (uint64_t)len * 8;
    for (int i = 7; i >= 0; i--) msg.push_back((uint8_t)((bits >> (i * 8)) & 0xff));

    for (size_t off = 0; off < msg.size(); off += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++)
            w[i] = ((uint32_t)msg[off + 4 * i] << 24) | ((uint32_t)msg[off + 4 * i + 1] << 16) |
                   ((uint32_t)msg[off + 4 * i + 2] << 8) | (uint32_t)msg[off + 4 * i + 3];
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ (~e & g);
            uint32_t t1 = hh + S1 + ch + K[i] + w[i];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + maj;
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    static const char* hexd = "0123456789abcdef";
    std::string out;
    for (int i = 0; i < 8; i++)
        for (int s = 28; s >= 0; s -= 4) out += hexd[(h[i] >> s) & 0xf];
    return out;
}

// ------------------------------------------------------------ known-bad hash list (uses our HashMap)
class BlockList {
    HashMap<std::string> map;
    size_t n;
public:
    BlockList() : map(1000), n(0) {}
    // one SHA-256 per line; '#' starts a comment
    bool load(const std::string& path) {
        std::ifstream f(path);
        if (!f) return false;
        std::string line;
        while (std::getline(f, line)) {
            size_t s = line.find_first_not_of(" \t\r");
            if (s == std::string::npos || line[s] == '#') continue;
            std::string tok = line.substr(s, 64);
            if (tok.size() != 64) continue;
            for (size_t i = 0; i < tok.size(); i++) tok[i] = (char)tolower((unsigned char)tok[i]);
            map.insert(strtoull(tok.substr(0, 16).c_str(), nullptr, 16), tok);
            n++;
        }
        return true;
    }
    bool contains(const std::string& hex) {
        std::string full;
        return map.get(strtoull(hex.substr(0, 16).c_str(), nullptr, 16), full) && full == hex;
    }
    size_t size() const { return n; }
};

// ------------------------------------------------------------ helpers
inline double entropy(const std::vector<uint8_t>& d) {
    if (d.empty()) return 0;
    double cnt[256] = {0};
    for (size_t i = 0; i < d.size(); i++) cnt[d[i]]++;
    double e = 0;
    for (int i = 0; i < 256; i++)
        if (cnt[i] > 0) { double p = cnt[i] / (double)d.size(); e -= p * std::log2(p); }
    return e;
}

inline bool hasBytes(const std::vector<uint8_t>& d, const char* needle) {
    return memmem(d.data(), d.size(), needle, strlen(needle)) != nullptr;
}

template <typename Ehdr, typename Phdr>
inline void checkLayout(const std::vector<uint8_t>& d, FileReport& r) {
    if (d.size() < sizeof(Ehdr)) { r.add(10, "its header is cut short (damaged or tampered with)"); return; }
    Ehdr eh;
    memcpy(&eh, d.data(), sizeof(eh));
    uint64_t entry = eh.e_entry;
    bool wx = false, entryInCode = false, entryInData = false;

    if (eh.e_phnum > 0 && (uint64_t)eh.e_phoff + (uint64_t)eh.e_phnum * sizeof(Phdr) <= d.size()) {
        for (unsigned i = 0; i < eh.e_phnum; i++) {
            Phdr ph;
            memcpy(&ph, d.data() + eh.e_phoff + (size_t)i * sizeof(Phdr), sizeof(ph));
            if (ph.p_type != PT_LOAD) continue;
            bool w = (ph.p_flags & PF_W) != 0, x = (ph.p_flags & PF_X) != 0;
            if (w && x) wx = true;
            if (entry >= ph.p_vaddr && entry < (uint64_t)ph.p_vaddr + ph.p_memsz) {
                if (x) entryInCode = true;
                if (w) entryInData = true;
            }
        }
    }
    if (wx) r.add(25, "has a memory area that is both writable and runnable, so it can rewrite its own code");
    if ((eh.e_type == ET_EXEC || eh.e_type == ET_DYN) && entry != 0) {
        if (entryInData) r.add(25, "starts running from a data area, a trick viruses use after injecting code");
        else if (!entryInCode) r.add(20, "its start point is not inside any code area");
    }
    if (eh.e_shnum == 0) r.add(10, "its section table is removed (common for packed or hand-edited files)");
    else if ((uint64_t)eh.e_shoff + (uint64_t)eh.e_shnum * eh.e_shentsize > d.size())
        r.add(15, "its section table points outside the file (corrupted or tampered with)");
}

struct Pattern { const char* text; int points; const char* why; };
static const Pattern PATTERNS[] = {
    {"/etc/ld.so.preload", 20, "mentions the file used to slip code into every program"},
    {"LD_PRELOAD",         10, "can force its own code into other programs"},
    {"/dev/tcp/",          20, "opens network connections the way remote-control shells do"},
    {"bash -i",            20, "starts an interactive shell (typical of remote-control tools)"},
    {"nc -e",              25, "hands a shell to someone over the network"},
    {"mkfifo /tmp",        15, "builds a hidden pipe, a classic reverse-shell ingredient"},
    {"authorized_keys",    15, "touches SSH keys, which could give an attacker permanent access"},
    {"/etc/shadow",        15, "reads the password database"},
    {"/etc/cron",          10, "can schedule itself to run again and again"},
    {"crontab",            10, "can schedule itself to run again and again"},
    {"/etc/rc.local",      10, "can install itself to start at boot"},
    {"/etc/init.d/",       10, "can install itself to start at boot"},
    {"chmod +x",           10, "makes other files runnable"},
    {"chmod 777",          10, "opens up file permissions for everyone"},
    {"wget http",          10, "downloads more files from the internet"},
    {"curl http",          10, "downloads more files from the internet"},
    {"rm -rf /",           25, "contains a command that can erase the whole system"},
    {"of=/dev/sd",         30, "contains a command that can overwrite a whole disk"},
    {"stratum+tcp",        25, "contains a crypto-mining pool address"},
    {"xmrig",              25, "mentions a well-known crypto miner"},
};

inline const char* DOC_EXT[] = {"pdf","doc","docx","xls","xlsx","ppt","pptx","txt","rtf","odt",
                                "jpg","jpeg","png","gif","bmp","mp3","mp4","avi","mkv","html"};

inline bool isElfFile(const std::string& path, struct stat& st) {
    if (lstat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 4) return false;
    std::ifstream f(path, std::ios::binary);
    char m[4];
    return f.read(m, 4) && m[0] == 0x7f && m[1] == 'E' && m[2] == 'L' && m[3] == 'F';
}

// Examine one file. Returns false when it is not an ELF file (nothing to report).
inline bool scanFile(const std::string& path, BlockList& bl, FileReport& r) {
    struct stat st;
    if (!isElfFile(path, st)) return false;
    r = FileReport();
    r.path = path;
    r.size = (unsigned long long)st.st_size;

    // --- disguise and permission tricks (USB specific)
    std::string name = path.substr(path.find_last_of('/') + 1);
    size_t dot = name.find_last_of('.');
    if (dot != std::string::npos) {
        std::string ext = name.substr(dot + 1);
        for (size_t i = 0; i < ext.size(); i++) ext[i] = (char)tolower((unsigned char)ext[i]);
        for (size_t i = 0; i < sizeof(DOC_EXT) / sizeof(DOC_EXT[0]); i++)
            if (ext == DOC_EXT[i]) { r.add(30, "pretends to be a ." + ext + " file but is really a program"); break; }
    }
    if (!name.empty() && name[0] == '.' && (st.st_mode & 0111)) r.add(10, "is a hidden program (name starts with a dot)");
    if (st.st_mode & (S_ISUID | S_ISGID)) r.add(25, "is set to run with extra (admin-level) powers");

    const unsigned long long MAX_SIZE = 64ULL * 1024 * 1024;
    if (r.size > MAX_SIZE) return true;   // too big to read closely

    std::vector<uint8_t> d((size_t)r.size);
    {
        std::ifstream f(path, std::ios::binary);
        if (!f.read((char*)d.data(), (std::streamsize)d.size())) return true;
    }
    r.sha256 = sha256Hex(d.data(), d.size());
    if (bl.contains(r.sha256)) r.add(100, "matches a KNOWN malware fingerprint on your block list");

    const bool hostLE = (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__);
    if (d.size() > EI_DATA && d[EI_DATA] == (hostLE ? ELFDATA2LSB : ELFDATA2MSB)) {
        if (d[EI_CLASS] == ELFCLASS64) checkLayout<Elf64_Ehdr, Elf64_Phdr>(d, r);
        else if (d[EI_CLASS] == ELFCLASS32) checkLayout<Elf32_Ehdr, Elf32_Phdr>(d, r);
        else r.add(10, "declares an invalid file class");
    }

    if (hasBytes(d, "UPX!")) r.add(30, "is squashed with UPX, a packer often used to hide malware");
    if (d.size() > 8192 && entropy(d) > 7.2) r.add(20, "looks scrambled or encrypted inside (very high randomness)");

    int strPoints = 0;   // suspicious text is capped so big legitimate tools are not over-punished
    for (size_t i = 0; i < sizeof(PATTERNS) / sizeof(PATTERNS[0]) && strPoints < 60; i++) {
        if (hasBytes(d, PATTERNS[i].text)) {
            int pts = std::min(PATTERNS[i].points, 60 - strPoints);
            strPoints += pts;
            r.add(pts, std::string(PATTERNS[i].why) + "  [\"" + PATTERNS[i].text + "\"]");
        }
    }
    return true;
}

// Visit every regular file below dir (symlinks are never followed).
inline void walk(const std::string& dir, int depth, const std::function<void(const std::string&)>& onFile) {
    if (depth > 20) return;
    DIR* dp = opendir(dir.c_str());
    if (!dp) return;
    while (dirent* e = readdir(dp)) {
        std::string n = e->d_name;
        if (n == "." || n == "..") continue;
        std::string p = dir + (dir.back() == '/' ? "" : "/") + n;
        struct stat st;
        if (lstat(p.c_str(), &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) walk(p, depth + 1, onFile);
        else if (S_ISREG(st.st_mode)) onFile(p);
    }
    closedir(dp);
}

} // namespace scan

#endif
