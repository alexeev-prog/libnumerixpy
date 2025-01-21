

def pure_calculate_discriminant(a: int, b: int, c: int) -> float:
	d = b * b - 4 * a * c

	return d


def fac(n):
	if n == 1:
		return 1
	return fac(n - 1) * n


def pure_cfactorial_sum(array: list):
	fac_sum = 0

	for n in array:
		n = int(n)

		fac_sum += fac(n)

	return fac_sum


def pure_ifactorial_sum(array: str):
	fac_sum = 0

	for n in list(array):
		n = int(n)

		fac_sum += fac(n)

	return fac_sum
