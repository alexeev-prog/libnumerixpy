#define PY_SSIZE_T_CLEAN
// #define Py_GIL_DISABLED
#include <Python.h>

/**
 * @brief      Execute a shell command
 *
 * @param      self  The object
 * @param      args  The arguments
 *
 * @return     status code
 */
static PyObject
*lnpy_exec_system(PyObject *self, PyObject *args)
{
	const char *command;
	int sts;

	if (!PyArg_ParseTuple(args, "s", &command)) {
		return NULL;
	}
	sts = system(command);

	return PyLong_FromLong(sts);
}

static PyMethodDef LNPYMethods[] = { { "lnpy_exec_system", lnpy_exec_system, METH_VARARGS,
									   "Execute a shell command." },
									 { NULL, NULL, 0, NULL } };

static struct PyModuleDef lnpy_base = { PyModuleDef_HEAD_INIT, "base", NULL, -1, LNPYMethods };

PyMODINIT_FUNC PyInit_base(void) { return PyModule_Create(&lnpy_base); }
