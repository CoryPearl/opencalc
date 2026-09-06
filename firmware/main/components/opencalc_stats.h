#ifndef OPENCALC_STATS_H
#define OPENCALC_STATS_H

#include <stdbool.h>

typedef enum {
    OPENCALC_STATS_TAIL_LESS = -1,
    OPENCALC_STATS_TAIL_NOT_EQUAL = 0,
    OPENCALC_STATS_TAIL_GREATER = 1,
} opencalc_stats_tail_t;

typedef struct {
    double estimate;
    double low;
    double high;
    double standard_error;
    double critical;
    double df;
} opencalc_stats_interval_t;

double opencalc_stats_normal_pdf(double x, double mean, double stdev);
double opencalc_stats_normal_cdf(double x, double mean, double stdev);
double opencalc_stats_inverse_normal(double probability, double mean, double stdev);
double opencalc_stats_t_pdf(double x, double df);
double opencalc_stats_t_cdf(double x, double df);
double opencalc_stats_inverse_t(double probability, double df);
double opencalc_stats_chi_square_pdf(double x, double df);
double opencalc_stats_chi_square_cdf(double x, double df);
double opencalc_stats_f_pdf(double x, double df1, double df2);
double opencalc_stats_f_cdf(double x, double df1, double df2);
double opencalc_stats_binomial_pdf(int trials, double probability, int successes);
double opencalc_stats_binomial_cdf(int trials, double probability, int successes);
double opencalc_stats_poisson_pdf(double mean, int count);
double opencalc_stats_poisson_cdf(double mean, int count);
double opencalc_stats_p_value(double cdf, opencalc_stats_tail_t tail);

bool opencalc_stats_z_interval(double mean, double sigma, int n, double confidence,
                               opencalc_stats_interval_t *result);
bool opencalc_stats_t_interval(double mean, double sample_stdev, int n, double confidence,
                               opencalc_stats_interval_t *result);
bool opencalc_stats_one_prop_interval(int successes, int n, double confidence,
                                      opencalc_stats_interval_t *result);
bool opencalc_stats_two_prop_interval(int successes1, int n1, int successes2, int n2,
                                      double confidence, opencalc_stats_interval_t *result);
bool opencalc_stats_two_mean_z_interval(double mean1, double sigma1, int n1,
                                        double mean2, double sigma2, int n2,
                                        double confidence, opencalc_stats_interval_t *result);
bool opencalc_stats_two_mean_t_interval(double mean1, double stdev1, int n1,
                                        double mean2, double stdev2, int n2,
                                        double confidence, opencalc_stats_interval_t *result);

#endif
