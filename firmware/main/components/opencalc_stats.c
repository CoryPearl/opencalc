#include "opencalc_stats.h"

#include <float.h>
#include <math.h>
#include <stddef.h>

#define STATS_PI 3.14159265358979323846264338327950288
#define STATS_EPSILON 3e-14
#define STATS_ITERATIONS 240

static double clamp_probability(double value)
{
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static double beta_fraction(double a, double b, double x)
{
    double qab = a + b;
    double qap = a + 1.0;
    double qam = a - 1.0;
    double c = 1.0;
    double d = 1.0 - qab * x / qap;
    if (fabs(d) < DBL_MIN) d = DBL_MIN;
    d = 1.0 / d;
    double result = d;

    for (int m = 1; m <= STATS_ITERATIONS; ++m) {
        int m2 = 2 * m;
        double aa = m * (b - m) * x / ((qam + m2) * (a + m2));
        d = 1.0 + aa * d;
        if (fabs(d) < DBL_MIN) d = DBL_MIN;
        c = 1.0 + aa / c;
        if (fabs(c) < DBL_MIN) c = DBL_MIN;
        d = 1.0 / d;
        result *= d * c;

        aa = -(a + m) * (qab + m) * x / ((a + m2) * (qap + m2));
        d = 1.0 + aa * d;
        if (fabs(d) < DBL_MIN) d = DBL_MIN;
        c = 1.0 + aa / c;
        if (fabs(c) < DBL_MIN) c = DBL_MIN;
        d = 1.0 / d;
        double delta = d * c;
        result *= delta;
        if (fabs(delta - 1.0) <= STATS_EPSILON) break;
    }
    return result;
}

static double regularized_beta(double x, double a, double b)
{
    if (a <= 0.0 || b <= 0.0 || x < 0.0 || x > 1.0) return NAN;
    if (x == 0.0) return 0.0;
    if (x == 1.0) return 1.0;
    double front = exp(lgamma(a + b) - lgamma(a) - lgamma(b) +
                       a * log(x) + b * log1p(-x));
    if (x < (a + 1.0) / (a + b + 2.0)) {
        return clamp_probability(front * beta_fraction(a, b, x) / a);
    }
    return clamp_probability(1.0 - front * beta_fraction(b, a, 1.0 - x) / b);
}

static double regularized_gamma_p(double a, double x)
{
    if (a <= 0.0 || x < 0.0) return NAN;
    if (x == 0.0) return 0.0;
    if (x < a + 1.0) {
        double term = 1.0 / a;
        double sum = term;
        double ap = a;
        for (int i = 1; i <= STATS_ITERATIONS; ++i) {
            ap += 1.0;
            term *= x / ap;
            sum += term;
            if (fabs(term) < fabs(sum) * STATS_EPSILON) break;
        }
        return clamp_probability(sum * exp(-x + a * log(x) - lgamma(a)));
    }

    double b = x + 1.0 - a;
    double c = 1.0 / DBL_MIN;
    double d = 1.0 / b;
    double h = d;
    for (int i = 1; i <= STATS_ITERATIONS; ++i) {
        double an = -(double)i * ((double)i - a);
        b += 2.0;
        d = an * d + b;
        if (fabs(d) < DBL_MIN) d = DBL_MIN;
        c = b + an / c;
        if (fabs(c) < DBL_MIN) c = DBL_MIN;
        d = 1.0 / d;
        double delta = d * c;
        h *= delta;
        if (fabs(delta - 1.0) <= STATS_EPSILON) break;
    }
    return clamp_probability(1.0 - exp(-x + a * log(x) - lgamma(a)) * h);
}

double opencalc_stats_normal_pdf(double x, double mean, double stdev)
{
    if (!(stdev > 0.0)) return NAN;
    double z = (x - mean) / stdev;
    return exp(-0.5 * z * z) / (stdev * sqrt(2.0 * STATS_PI));
}

double opencalc_stats_normal_cdf(double x, double mean, double stdev)
{
    if (!(stdev > 0.0)) return NAN;
    return clamp_probability(0.5 * erfc(-(x - mean) / (stdev * sqrt(2.0))));
}

double opencalc_stats_inverse_normal(double probability, double mean, double stdev)
{
    if (!(probability > 0.0 && probability < 1.0 && stdev > 0.0)) return NAN;
    double lo = mean - 40.0 * stdev;
    double hi = mean + 40.0 * stdev;
    for (int i = 0; i < 100; ++i) {
        double mid = 0.5 * (lo + hi);
        if (opencalc_stats_normal_cdf(mid, mean, stdev) < probability) lo = mid;
        else hi = mid;
    }
    return 0.5 * (lo + hi);
}

double opencalc_stats_t_pdf(double x, double df)
{
    if (!(df > 0.0)) return NAN;
    double log_value = lgamma((df + 1.0) * 0.5) - lgamma(df * 0.5) -
        0.5 * log(df * STATS_PI) - 0.5 * (df + 1.0) * log1p(x * x / df);
    return exp(log_value);
}

double opencalc_stats_t_cdf(double x, double df)
{
    if (!(df > 0.0)) return NAN;
    if (x == 0.0) return 0.5;
    double beta = regularized_beta(df / (df + x * x), df * 0.5, 0.5);
    return clamp_probability(x > 0.0 ? 1.0 - 0.5 * beta : 0.5 * beta);
}

double opencalc_stats_inverse_t(double probability, double df)
{
    if (!(probability > 0.0 && probability < 1.0 && df > 0.0)) return NAN;
    double lo = -1.0;
    double hi = 1.0;
    while (opencalc_stats_t_cdf(lo, df) > probability && lo > -1e12) lo *= 2.0;
    while (opencalc_stats_t_cdf(hi, df) < probability && hi < 1e12) hi *= 2.0;
    for (int i = 0; i < 110; ++i) {
        double mid = 0.5 * (lo + hi);
        if (opencalc_stats_t_cdf(mid, df) < probability) lo = mid;
        else hi = mid;
    }
    return 0.5 * (lo + hi);
}

double opencalc_stats_chi_square_pdf(double x, double df)
{
    if (x < 0.0 || !(df > 0.0)) return NAN;
    if (x == 0.0) return df == 2.0 ? 0.5 : (df < 2.0 ? INFINITY : 0.0);
    double k = 0.5 * df;
    return exp((k - 1.0) * log(x) - 0.5 * x - k * log(2.0) - lgamma(k));
}

double opencalc_stats_chi_square_cdf(double x, double df)
{
    if (x < 0.0 || !(df > 0.0)) return NAN;
    return regularized_gamma_p(0.5 * df, 0.5 * x);
}

double opencalc_stats_f_pdf(double x, double df1, double df2)
{
    if (x < 0.0 || !(df1 > 0.0 && df2 > 0.0)) return NAN;
    if (x == 0.0) return df1 == 2.0 ? 1.0 : (df1 < 2.0 ? INFINITY : 0.0);
    double a = 0.5 * df1;
    double b = 0.5 * df2;
    return exp(a * log(df1 / df2) + (a - 1.0) * log(x) -
               (a + b) * log1p(df1 * x / df2) - lgamma(a) - lgamma(b) + lgamma(a + b));
}

double opencalc_stats_f_cdf(double x, double df1, double df2)
{
    if (x < 0.0 || !(df1 > 0.0 && df2 > 0.0)) return NAN;
    return regularized_beta((df1 * x) / (df1 * x + df2), 0.5 * df1, 0.5 * df2);
}

double opencalc_stats_binomial_pdf(int trials, double probability, int successes)
{
    if (trials < 0 || successes < 0 || successes > trials ||
        probability < 0.0 || probability > 1.0) return NAN;
    if (probability == 0.0) return successes == 0 ? 1.0 : 0.0;
    if (probability == 1.0) return successes == trials ? 1.0 : 0.0;
    double log_value = lgamma(trials + 1.0) - lgamma(successes + 1.0) -
        lgamma(trials - successes + 1.0) + successes * log(probability) +
        (trials - successes) * log1p(-probability);
    return exp(log_value);
}

double opencalc_stats_binomial_cdf(int trials, double probability, int successes)
{
    if (trials < 0 || probability < 0.0 || probability > 1.0) return NAN;
    if (successes < 0) return 0.0;
    if (successes >= trials) return 1.0;
    return regularized_beta(1.0 - probability, trials - successes, successes + 1.0);
}

double opencalc_stats_poisson_pdf(double mean, int count)
{
    if (!(mean > 0.0) || count < 0) return NAN;
    return exp(-mean + count * log(mean) - lgamma(count + 1.0));
}

double opencalc_stats_poisson_cdf(double mean, int count)
{
    if (!(mean > 0.0)) return NAN;
    if (count < 0) return 0.0;
    return clamp_probability(1.0 - regularized_gamma_p(count + 1.0, mean));
}

double opencalc_stats_p_value(double cdf, opencalc_stats_tail_t tail)
{
    if (!isfinite(cdf)) return NAN;
    cdf = clamp_probability(cdf);
    if (tail == OPENCALC_STATS_TAIL_LESS) return cdf;
    if (tail == OPENCALC_STATS_TAIL_GREATER) return 1.0 - cdf;
    return fmin(1.0, 2.0 * fmin(cdf, 1.0 - cdf));
}

static bool interval_finish(double estimate, double standard_error, double critical, double df,
                            opencalc_stats_interval_t *result)
{
    if (result == NULL || !isfinite(estimate) || !(standard_error >= 0.0) ||
        !isfinite(critical)) return false;
    result->estimate = estimate;
    result->standard_error = standard_error;
    result->critical = critical;
    result->df = df;
    result->low = estimate - critical * standard_error;
    result->high = estimate + critical * standard_error;
    return true;
}

static bool valid_confidence(double confidence)
{
    return confidence > 0.0 && confidence < 1.0;
}

bool opencalc_stats_z_interval(double mean, double sigma, int n, double confidence,
                               opencalc_stats_interval_t *result)
{
    if (!(sigma > 0.0) || n <= 0 || !valid_confidence(confidence)) return false;
    return interval_finish(mean, sigma / sqrt((double)n),
        opencalc_stats_inverse_normal(0.5 + 0.5 * confidence, 0.0, 1.0), INFINITY, result);
}

bool opencalc_stats_t_interval(double mean, double sample_stdev, int n, double confidence,
                               opencalc_stats_interval_t *result)
{
    if (!(sample_stdev > 0.0) || n < 2 || !valid_confidence(confidence)) return false;
    double df = n - 1.0;
    return interval_finish(mean, sample_stdev / sqrt((double)n),
        opencalc_stats_inverse_t(0.5 + 0.5 * confidence, df), df, result);
}

bool opencalc_stats_one_prop_interval(int successes, int n, double confidence,
                                      opencalc_stats_interval_t *result)
{
    if (n <= 0 || successes < 0 || successes > n || !valid_confidence(confidence)) return false;
    double p = (double)successes / n;
    return interval_finish(p, sqrt(p * (1.0 - p) / n),
        opencalc_stats_inverse_normal(0.5 + 0.5 * confidence, 0.0, 1.0), INFINITY, result);
}

bool opencalc_stats_two_prop_interval(int successes1, int n1, int successes2, int n2,
                                      double confidence, opencalc_stats_interval_t *result)
{
    if (n1 <= 0 || n2 <= 0 || successes1 < 0 || successes1 > n1 ||
        successes2 < 0 || successes2 > n2 || !valid_confidence(confidence)) return false;
    double p1 = (double)successes1 / n1;
    double p2 = (double)successes2 / n2;
    double se = sqrt(p1 * (1.0 - p1) / n1 + p2 * (1.0 - p2) / n2);
    return interval_finish(p1 - p2, se,
        opencalc_stats_inverse_normal(0.5 + 0.5 * confidence, 0.0, 1.0), INFINITY, result);
}

bool opencalc_stats_two_mean_z_interval(double mean1, double sigma1, int n1,
                                        double mean2, double sigma2, int n2,
                                        double confidence, opencalc_stats_interval_t *result)
{
    if (!(sigma1 > 0.0 && sigma2 > 0.0) || n1 <= 0 || n2 <= 0 ||
        !valid_confidence(confidence)) return false;
    double se = sqrt(sigma1 * sigma1 / n1 + sigma2 * sigma2 / n2);
    return interval_finish(mean1 - mean2, se,
        opencalc_stats_inverse_normal(0.5 + 0.5 * confidence, 0.0, 1.0), INFINITY, result);
}

bool opencalc_stats_two_mean_t_interval(double mean1, double stdev1, int n1,
                                        double mean2, double stdev2, int n2,
                                        double confidence, opencalc_stats_interval_t *result)
{
    if (!(stdev1 > 0.0 && stdev2 > 0.0) || n1 < 2 || n2 < 2 ||
        !valid_confidence(confidence)) return false;
    double v1 = stdev1 * stdev1 / n1;
    double v2 = stdev2 * stdev2 / n2;
    double df = (v1 + v2) * (v1 + v2) /
        (v1 * v1 / (n1 - 1.0) + v2 * v2 / (n2 - 1.0));
    return interval_finish(mean1 - mean2, sqrt(v1 + v2),
        opencalc_stats_inverse_t(0.5 + 0.5 * confidence, df), df, result);
}
