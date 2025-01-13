#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <stdio.h>
#include <libbasemath.h>

/**
 * @brief      Calculates the discriminant.
 *
 * @param      self  The object
 * @param      args  The arguments
 *
 * @return     The discriminant.
 */
static PyObject
*Py_calculate_discriminant(PyObject *self, PyObject *args) {
	double a, b, c;

	if (!PyArg_ParseTuple(args, "ddd", &a, &b, &c)) {
		return NULL;
	}

	double discriminant = calculate_discriminant(a, b, c);

	return Py_BuildValue("d", discriminant);
}

static PyObject
*cFactorial_sum(PyObject *self, PyObject *args) {
	char *char_nums;
	if (!PyArg_ParseTuple(args, "s", &char_nums)) {
		return NULL;
	}

	unsigned long fact_sum;
	fact_sum = cfactorial_sum(char_nums);

	return Py_BuildValue("i", fact_sum);
}

static PyObject
*iFactorial_sum(PyObject *self, PyObject *args) {
	PyObject *lst;
	if (!PyArg_ParseTuple(args, "O", &lst)) {
		return NULL;
	}

	int n = PyObject_Length(lst);
	if (n < 0) {
		return NULL;
	}

	long nums[n];
	for (int i = 0; i < n; i++) {
		PyObject *item = PyList_GetItem(lst, i);
		long num = PyLong_AsLong(item);
		nums[i] = num;
	}

	unsigned long fact_sum;
	fact_sum = ifactorial_sum(nums, n);

	return Py_BuildValue("i", fact_sum);
}


static PyMethodDef LNPYMethods[] = { { "calculate_discriminant", Py_calculate_discriminant, METH_VARARGS,
									   "Calculate the discriminant by formula: D = b^2 * 4ac" },
									 { "ifactorial_sum", iFactorial_sum, METH_VARARGS,
									   "Calculate the iFactorial sum (from list of ints)" },
									 { "cfactorial_sum", cFactorial_sum, METH_VARARGS,
									   "Calculate the cFactorial sum (from digits in string of numbers)" },
									 { NULL, NULL, 0, NULL } };

static struct PyModuleDef lnpy_basemath = { PyModuleDef_HEAD_INIT, "math", "Libnumerixpy - BaseMath", -1, LNPYMethods };

PyMODINIT_FUNC PyInit_basemath(void) { return PyModule_Create(&lnpy_basemath); }

