import timeit
from functools import wraps

from purepython import (
    pure_calculate_discriminant,
    pure_cfactorial_sum,
    pure_ifactorial_sum,
)

from libnumerixpy.math import calculate_discriminant, cfactorial_sum, ifactorial_sum


def timing(f):
    @wraps(f)
    def wrapper(*args, **kwargs):
        start_time = timeit.default_timer()
        result = f(*args, **kwargs)
        ellapsed_time = timeit.default_timer() - start_time
        return result, ellapsed_time

    return wrapper


@timing
def pure_python():
    d = pure_calculate_discriminant(1.0, -3.0, 1.0)
    assert d == 5.0
    assert pure_cfactorial_sum("12345") == 153
    assert pure_ifactorial_sum([1, 2, 3, 4, 5]) == 153


@timing
def c_extension():
    d = calculate_discriminant(1.0, -3.0, 1.0)
    assert d == 5.0
    assert cfactorial_sum("12345") == 153
    assert ifactorial_sum([1, 2, 3, 4, 5]) == 153


def ppure_python():
    d = pure_calculate_discriminant(1.0, -3.0, 1.0)
    assert d == 5.0
    assert pure_cfactorial_sum("12345") == 153
    assert pure_ifactorial_sum([1, 2, 3, 4, 5]) == 153


def pc_extension():
    d = calculate_discriminant(1.0, -3.0, 1.0)
    assert d == 5.0
    assert cfactorial_sum("12345") == 153
    assert ifactorial_sum([1, 2, 3, 4, 5]) == 153


_, ellapsed_time = pure_python()
_, ellapsed_time2 = c_extension()

print(f"[PURE PYTHON] Elapsed time: {ellapsed_time}")
print(f"[C EXTENSION] Elapsed time: {ellapsed_time2}")

sum_1 = []
sum_2 = []

average_1 = 0
average_2 = 0

for i in range(100):
    execution_time = timeit.timeit(
        ppure_python, number=10000
    )  # указывается количество запусков функции
    print("Среднее время выполнения функции pure_python:", execution_time)

    sum_1.append(execution_time)

    execution_time2 = timeit.timeit(
        pc_extension, number=10000
    )  # указывается количество запусков функции
    print("Среднее время выполнения функции c_extension:", execution_time2)

    sum_2.append(execution_time2)

average_1 = sum(sum_1) / len(sum_1)
average_2 = sum(sum_2) / len(sum_2)

print(f"Среднее pure_python: {average_1}")
print(f"Среднее c_extension: {average_2}")
