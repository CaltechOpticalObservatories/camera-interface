//
// Created by hsdev on 6/3/25.
//

#ifndef TIMINGSTATS_H
#define TIMINGSTATS_H



#include <vector>
#include <numeric>
#include <cmath>
#include <algorithm>

class TimingStats {
public:
    std::vector<double> durations_us;

    void add(double duration_us) {
        durations_us.push_back(duration_us);
    }

    double mean() const {
        return std::accumulate(durations_us.begin(), durations_us.end(), 0.0) / durations_us.size();
    }

    double stddev(double mean_val) const {
        double sum_sq = 0.0;
        for (double d : durations_us) sum_sq += (d - mean_val) * (d - mean_val);
        return sqrt(sum_sq / durations_us.size());
    }

    double median() const {
        std::vector<double> sorted = durations_us;
        std::sort(sorted.begin(), sorted.end());
        size_t mid = sorted.size() / 2;
        return (sorted.size() % 2 == 0) ? (sorted[mid-1] + sorted[mid]) / 2.0 : sorted[mid];
    }

    double jitter() const {
        double mean_val = mean();
        return stddev(mean_val);
    }

    double hertz() const {
        return 1e6 / mean();
    }

    double hertz_stddev() const {
        double stddev_us = jitter();
        double mean_us = mean();
        return 1e6 * stddev_us / (mean_us * mean_us);
    }
};



#endif //TIMINGSTATS_H
