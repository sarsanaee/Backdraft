#ifndef _PERCENTILE_H
#define _PERCENTILE_H

#define r2 1.4142

struct p_hist {
	int count_bucket;
	int count_values;
	int *buckets;
};

struct p_hist *new_p_hist(int count_bucket);
void free_p_hist(struct p_hist *hist);
void add_number_to_p_hist(struct p_hist *hist, float value);

/*
 * percentile: should be in range (0, 1)
 * */
float get_percentile(struct p_hist *hist, float percentile);
#endif
