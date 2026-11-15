// Usb.h - Find USB storage devices through /sys and /proc, and mount them safely (read-only, no-exec).
//PENDING FEATURE: INCOMPLETE CODE
#ifndef LARPOS_USB_H
#define LARPOS_USB_H

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <map>
#include <string>
#include <sys/mount.h>
#include <sys/stat.h>
#include <vector>

namespace usb {

struct Partition {
    std::string dev;           // e.g. /dev/sdb1
    std::string mountPoint;    // empty if not mounted
    bool mountedByUs = false;
};

struct Drive {
    std::string name;          // e.g. sdb
    std::string label;         // vendor + model
    unsigned long long sizeMB = 0;
    std::vector<Partition> parts;
};

inline std::string readLine(const std::string& path) {
    std::ifstream f(path);
    std::string s;
    std::getline(f, s);
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

inline std::vector<std::string> listDir(const std::string& path) {
    std::vector<std::string> out;
    DIR* d = opendir(path.c_str());
    if (!d) return out;
    while (dirent* e = readdir(d)) {
        std::string n = e->d_name;
        if (n != "." && n != "..") out.push_back(n);
    }
    closedir(d);
    return out;
}

// /proc/mounts escapes spaces as \040 etc.
inline std::string unescapeMount(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 3 < s.size() && isdigit((unsigned char)s[i + 1])) {
            out += (char)strtol(s.substr(i + 1, 3).c_str(), nullptr, 8);
            i += 3;
        } else out += s[i];
    }
    return out;
}

inline std::map<std::string, std::string> readMounts() {
    std::map<std::string, std::string> m;
    std::ifstream f("/proc/mounts");
    std::string dev, mnt, rest;
    while (f >> dev >> mnt && std::getline(f, rest))
        if (!m.count(dev)) m[dev] = unescapeMount(mnt);
    return m;
}

// A block device counts as USB when its sysfs path runs through a usb bus.
inline std::vector<Drive> detect() {
    std::vector<Drive> drives;
    std::map<std::string, std::string> mounts = readMounts();
    std::vector<std::string> names = listDir("/sys/block");
    for (size_t i = 0; i < names.size(); i++) {
        const std::string& name = names[i];
        char real[PATH_MAX];
        if (!realpath(("/sys/block/" + name).c_str(), real)) continue;
        if (std::string(real).find("/usb") == std::string::npos) continue;

        Drive d;
        d.name = name;
        std::string vendor = readLine("/sys/block/" + name + "/device/vendor");
        std::string model = readLine("/sys/block/" + name + "/device/model");
        d.label = vendor.empty() ? model : (model.empty() ? vendor : vendor + " " + model);
        if (d.label.empty()) d.label = "USB storage";
        d.sizeMB = strtoull(readLine("/sys/block/" + name + "/size").c_str(), nullptr, 10) * 512ULL / (1024ULL * 1024ULL);

        std::vector<std::string> kids = listDir("/sys/block/" + name);
        for (size_t k = 0; k < kids.size(); k++) {
            if (kids[k].size() > name.size() && kids[k].compare(0, name.size(), name) == 0) {
                Partition p;
                p.dev = "/dev/" + kids[k];
                d.parts.push_back(p);
            }
        }
        if (d.parts.empty()) { Partition p; p.dev = "/dev/" + name; d.parts.push_back(p); }   // no partition table
        std::sort(d.parts.begin(), d.parts.end(), [](const Partition& a, const Partition& b) { return a.dev < b.dev; });
        for (size_t k = 0; k < d.parts.size(); k++) {
            std::map<std::string, std::string>::iterator it = mounts.find(d.parts[k].dev);
            if (it != mounts.end()) d.parts[k].mountPoint = it->second;
        }
        drives.push_back(d);
    }
    return drives;
}

// Mount read-only, no programs may run from it, no device files, no setuid. Needs root.
inline bool mountReadOnly(Partition& p, std::string& err) {
    mkdir("/mnt", 0755);
    mkdir("/mnt/larpos", 0755);
    std::string target = "/mnt/larpos/" + p.dev.substr(p.dev.find_last_of('/') + 1);
    mkdir(target.c_str(), 0700);
    static const char* TYPES[] = {"vfat", "exfat", "ext4", "ext3", "ext2", "ntfs3", "ntfs", "iso9660", "udf"};
    for (size_t i = 0; i < sizeof(TYPES) / sizeof(TYPES[0]); i++) {
        if (mount(p.dev.c_str(), target.c_str(), TYPES[i], MS_RDONLY | MS_NOSUID | MS_NODEV | MS_NOEXEC, nullptr) == 0) {
            p.mountPoint = target;
            p.mountedByUs = true;
            return true;
        }
    }
    err = strerror(errno);
    rmdir(target.c_str());
    return false;
}

inline void unmount(Partition& p) {
    if (!p.mountedByUs) return;
    umount(p.mountPoint.c_str());
    rmdir(p.mountPoint.c_str());
    p.mountPoint.clear();
    p.mountedByUs = false;
}

} // namespace usb

#endif // LARPOS_USB_H
