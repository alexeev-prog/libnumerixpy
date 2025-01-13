from libnumerixpy import lnpy_exec_system
from libnumerixpy.math import calculate_discriminant, cfactorial_sum, ifactorial_sum


def test_base():
	status_code = lnpy_exec_system('echo "Hello, World"')
	assert status_code == 0


def test_basemath():
	d = calculate_discriminant(1.0, -3.0, 1.0)
	assert d == 5.0
	assert cfactorial_sum("12345") == 153
	assert ifactorial_sum([1,2,3,4,5]) == 153
