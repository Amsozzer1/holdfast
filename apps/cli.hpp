#pragma once

// Minimal "--key value" / "--flag" parser shared by the binaries.

#include <cstdlib>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace holdfast::cli {

class Args {
public:
    Args(int argc, char** argv) {
        for (int i = 1; i < argc; ++i) {
            std::string k = argv[i];
            if (k.rfind("--", 0) != 0) { std::cerr << "unexpected argument: " << k << "\n"; std::exit(2); }
            k = k.substr(2);
            if (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0) kv_[k] = argv[++i];
            else kv_[k] = "1";
        }
    }
    [[nodiscard]] bool has(const std::string& k) const { return kv_.count(k) > 0; }
    [[nodiscard]] std::string get(const std::string& k, const std::string& def = "") const {
        auto it = kv_.find(k);
        return it == kv_.end() ? def : it->second;
    }
    [[nodiscard]] double num(const std::string& k, double def) const { return has(k) ? std::stod(get(k)) : def; }
    [[nodiscard]] std::vector<std::string> list(const std::string& k, const std::string& def) const {
        std::vector<std::string> out;
        std::stringstream ss(get(k, def));
        for (std::string item; std::getline(ss, item, ',');) if (!item.empty()) out.push_back(item);
        return out;
    }

private:
    std::map<std::string, std::string> kv_;
};

}  // namespace holdfast::cli
