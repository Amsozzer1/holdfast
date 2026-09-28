#pragma once

#include <chrono>
#include <map>
#include <string>
#include <vector>

namespace holdfast {

// Per-stage wall-clock samples with p50/p95 summaries.
class StageTimer {
public:
    class Scope {
    public:
        Scope(StageTimer& t, std::string stage) : t_(t), stage_(std::move(stage)), start_(std::chrono::steady_clock::now()) {}
        ~Scope() { t_.add(stage_, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start_).count()); }
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

    private:
        StageTimer& t_;
        std::string stage_;
        std::chrono::steady_clock::time_point start_;
    };

    Scope scope(std::string stage) { return {*this, std::move(stage)}; }
    void add(const std::string& stage, double ms);

    struct Summary { std::string stage; double p50, p95, mean; size_t n; };
    [[nodiscard]] std::vector<Summary> summary() const;  // in insertion order
    [[nodiscard]] std::string table() const;

private:
    std::vector<std::string> order_;
    std::map<std::string, std::vector<double>> samples_;
};

}  // namespace holdfast
