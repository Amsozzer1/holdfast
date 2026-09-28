#pragma once

#include <cmath>
#include <cstdint>
#include <numbers>
#include <random>

namespace holdfast {

// Portable seeded RNG. std::mt19937_64's raw output is fixed by the standard, but the
// std:: distributions are not (libc++ and libstdc++ differ), so the distributions are
// implemented here. Same seed => same stream on macOS and Linux.
class Rng {
public:
    explicit Rng(std::uint64_t seed) : eng_(seed) {}

    double uniform() { return static_cast<double>(eng_() >> 11) * 0x1.0p-53; }  // [0, 1)
    double uniform(double a, double b) { return a + (b - a) * uniform(); }
    int uniform_int(int lo, int hi) {  // inclusive
        return lo + static_cast<int>(uniform() * (hi - lo + 1));
    }
    bool bernoulli(double p) { return uniform() < p; }
    double normal(double mean = 0.0, double sd = 1.0) {  // Box-Muller
        double u1 = uniform();
        while (u1 <= 0.0) u1 = uniform();
        const double u2 = uniform();
        return mean + sd * std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * std::numbers::pi * u2);
    }
    int poisson(double lambda) {  // Knuth; fine for small lambda
        const double L = std::exp(-lambda);
        int k = 0;
        double p = 1.0;
        do { ++k; p *= uniform(); } while (p > L);
        return k - 1;
    }

private:
    std::mt19937_64 eng_;
};

}  // namespace holdfast
