#include	<stdlib.h>
#include	<Python.h>

int main(void)
{
	PyObject *mod = NULL, *func = NULL;

	Py_Initialize();
	if (!Py_IsInitialized())
		return EXIT_FAILURE;
	PyGILState_STATE state = PyGILState_Ensure();

	PyRun_SimpleString("import sys");
	PyRun_SimpleString("sys.path.append('.')");
	mod = PyImport_ImportModule("demo");

	// call a function
	func = PyObject_GetAttrString(mod, "hello");
	if (!PyCallable_Check(func))
		return EXIT_FAILURE;
	PyObject_CallFunction(func, NULL);

	// call func in a class
	PyObject *cls = PyObject_GetAttrString(mod, "Hello");
	PyObject *arg = Py_BuildValue("(i)", 3);
	PyObject *cls_inst = PyObject_Call(cls, arg, NULL);
	PyObject_CallMethod(cls_inst, "run", "i", 1);

	PyGILState_Release(state);
	Py_Finalize();
	return EXIT_SUCCESS;
}
