#define PY_SSIZE_T_CLEAN
// #define Py_GIL_DISABLED
#include <Python.h>

static PyObject
*calculate_discriminant(PyObject *self, PyObject *args) {
	double a, b, c;

	if (!PyArg_ParseTuple(args, "ddd", &a, &b, &c)) {
		return NULL;
	}

	double discriminant = b * b - 4 * a * c;

	return Py_BuildValue("d", discriminant);
}

static PyMethodDef LNPYMethods[] = { { "calculate_discriminant", calculate_discriminant, METH_VARARGS,
									   "Calculate the discriminant: D = b^2 * 4ac" },
									 { NULL, NULL, 0, NULL } };

static struct PyModuleDef lnpy_cmath = { PyModuleDef_HEAD_INIT, "cmath", NULL, -1, LNPYMethods };

PyMODINIT_FUNC PyInit_cmath(void) { return PyModule_Create(&lnpy_cmath); }

