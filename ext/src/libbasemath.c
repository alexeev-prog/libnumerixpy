#include "libbasemath.h"

double calculate_discriminant(double a, double b, double c) {
	double discriminant = b * b - 4 * a * c;

	return discriminant;
}

unsigned long factorial(long n) {
	if (n == 0)
		return 1;
	
	return (unsigned)n * factorial(n-1);
}

unsigned long cfactorial_sum(char num_chars[]) {
	unsigned long fact_num;
	unsigned long sum = 0;

	for (int i = 0; num_chars[i]; i++) {
		int ith_num = num_chars[i] - '0';
		fact_num = factorial(ith_num);
		sum = sum + fact_num;
	}
	return sum;
}

unsigned long ifactorial_sum(long nums[], int size) {
	unsigned long fact_num;
	unsigned long sum = 0;
	for (int i = 0; i < size; i++) {
		fact_num = factorial(nums[i]);
		sum += fact_num;
	}
	return sum;
}
