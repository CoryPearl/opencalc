#include "opencalc_stats.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static void close_to(double actual, double expected, double tolerance)
{
    if (!isfinite(actual) || fabs(actual - expected) > tolerance) {
        fprintf(stderr, "expected %.12g, got %.12g\n", expected, actual);
        assert(0);
    }
}

int main(void)
{
    close_to(opencalc_stats_normal_cdf(0.0, 0.0, 1.0), 0.5, 1e-13);
    close_to(opencalc_stats_inverse_normal(0.975, 0.0, 1.0), 1.95996398454, 1e-9);
    close_to(opencalc_stats_t_cdf(2.22813885196, 10.0), 0.975, 1e-9);
    close_to(opencalc_stats_inverse_t(0.975, 10.0), 2.22813885196, 1e-8);
    close_to(opencalc_stats_inverse_t(0.999, 1.0), 318.308838986, 1e-6);
    close_to(opencalc_stats_chi_square_cdf(9.48772903678, 4.0), 0.95, 1e-9);
    close_to(opencalc_stats_f_cdf(3.32583453041, 5.0, 10.0), 0.95, 1e-8);
    close_to(opencalc_stats_binomial_pdf(10, 0.5, 5), 0.24609375, 1e-13);
    close_to(opencalc_stats_binomial_cdf(10, 0.5, 5), 0.623046875, 1e-13);
    close_to(opencalc_stats_poisson_pdf(3.0, 2), 0.224041807655, 1e-12);
    close_to(opencalc_stats_poisson_cdf(3.0, 2), 0.423190081127, 1e-12);

    close_to(opencalc_stats_p_value(0.02, OPENCALC_STATS_TAIL_LESS), 0.02, 1e-15);
    close_to(opencalc_stats_p_value(0.98, OPENCALC_STATS_TAIL_GREATER), 0.02, 1e-15);
    close_to(opencalc_stats_p_value(0.02, OPENCALC_STATS_TAIL_NOT_EQUAL), 0.04, 1e-15);

    opencalc_stats_interval_t interval;
    assert(opencalc_stats_z_interval(10.0, 2.0, 25, 0.95, &interval));
    close_to(interval.low, 9.21601440618, 1e-9);
    close_to(interval.high, 10.7839855938, 1e-9);
    assert(opencalc_stats_t_interval(10.0, 2.0, 25, 0.95, &interval));
    close_to(interval.df, 24.0, 1e-12);
    close_to(interval.low, 9.17444057535, 1e-8);
    assert(opencalc_stats_one_prop_interval(56, 100, 0.95, &interval));
    close_to(interval.estimate, 0.56, 1e-15);
    assert(opencalc_stats_two_prop_interval(56, 100, 42, 100, 0.95, &interval));
    close_to(interval.estimate, 0.14, 1e-15);
    assert(opencalc_stats_two_mean_t_interval(12.0, 2.5, 20, 10.0, 3.0, 24, 0.95, &interval));
    assert(interval.df > 40.0 && interval.df < 42.0);

    assert(isnan(opencalc_stats_normal_cdf(0.0, 0.0, 0.0)));
    assert(isnan(opencalc_stats_t_cdf(0.0, 0.0)));
    assert(isnan(opencalc_stats_chi_square_cdf(-1.0, 4.0)));
    assert(isnan(opencalc_stats_binomial_pdf(5, 1.2, 2)));
    assert(!opencalc_stats_z_interval(0.0, 1.0, 0, 0.95, &interval));
    assert(!opencalc_stats_t_interval(0.0, 0.0, 10, 0.95, &interval));
    assert(!opencalc_stats_one_prop_interval(11, 10, 0.95, &interval));
    assert(!opencalc_stats_two_prop_interval(1, 0, 1, 2, 0.95, &interval));

    close_to(opencalc_stats_normal_cdf(-8.0, 0.0, 1.0), 6.22096057427e-16, 1e-27);
    close_to(opencalc_stats_normal_cdf(8.0, 0.0, 1.0), 0.9999999999999993, 1e-15);
    puts("stats regression tests passed");
    return 0;
}
