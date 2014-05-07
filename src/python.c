#include "python2.7/Python.h"
#include "quakedef.h"


PyObject *dict_var;
PyObject *dict_stats_self;

struct PyFunctions_s
{
	PyObject *function;
	struct PyFunctions_s *next;
} ;


static PyObject *Python_Create_Variable(PyObject * self, PyObject * args)
{
	char *s;
	char *s1;
	cvar_t *var;

	if (!PyArg_ParseTuple(args, "ss", &s, &s1))
	{
		Py_RETURN_NONE;
	}


	var = malloc(sizeof(cvar_t));
	memset(var, 0, sizeof(cvar_t));
	var->name = strdup(s);
	var->defaultvalue = strdup(s1);


	Cvar_Register(var);
	Py_RETURN_NONE;

}

static PyObject *Python_Com_Printf(PyObject * self, PyObject * args)
{
	char *s;

	if (!PyArg_ParseTuple(args, "s", &s))
	{
		Py_RETURN_NONE;
	}
	Com_Printf("%s\n", s);

	Py_RETURN_NONE;
}

static PyObject *Python_Cbuf_AddText(PyObject * self, PyObject * args)
{
	char *s;

	if (!PyArg_ParseTuple(args, "s", &s))
	{
		Py_RETURN_NONE;
	}
	Cbuf_AddText(s);
	Py_RETURN_NONE;
}


static PyMethodDef Console_methods[] = {
	{"Com_Printf", (PyCFunction) Python_Com_Printf, METH_VARARGS, "Calls Com_Printf from python\n"},
	{"Cbuf_AddText", (PyCFunction) Python_Cbuf_AddText, METH_VARARGS, "Calls Cbuf_AddText from python\n"},
	{"Create_Variable", (PyCFunction) Python_Create_Variable, METH_VARARGS, "Creates a variable !\n"},
	{NULL, NULL, 0, NULL}

};

//PyDict_SetItemString(
void Python_Call(char *module, char *function, PyObject * arguments);


int python_initialized;
struct PyFunctions_s *Python_Functions = NULL;

void Python_Relay_Console_To_Python(char *s){
	PyObject *pargs,*val;
	pargs = PyTuple_New(1);
	val = Py_BuildValue("s",s);
	PyTuple_SetItem(pargs,0,val);
	Python_Call("main","AddConsoleLine",pargs);


}

void Python_Hooks(void)
{


	Py_InitModule("console", Console_methods);

}

void Python_Load(char *file)
{
	PyObject *pName, *pModule, *pFunc;
	struct PyFunctions_s *temp;


	if (!python_initialized)
		return;

	pName = PyString_FromString(file);

	pModule = PyImport_Import(pName);
	Py_DECREF(pName);

	if (pModule == NULL)
	{
		Com_Printf("Python - Loading %s failed\n", file);
		return;
	}
	else
	{
		Com_Printf("Python - Loaded %s \n", PyModule_GetName(pModule));
	}

	if (Python_Functions == NULL)
	{
		Python_Functions = (struct PyFunctions_s *) malloc(sizeof(struct PyFunctions_s));
		memset(Python_Functions, 0, sizeof(struct PyFunctions_s));
		Python_Functions->function = pModule;
	}
	else
	{
		temp = Python_Functions;
		while (temp->next)
			temp = temp->next;
		temp->next = (struct PyFunctions_s *) malloc(sizeof(struct PyFunctions_s));
		memset(temp->next, 0, sizeof(struct PyFunctions_s));
		temp->next->function = pModule;

	}



	PyObject_SetAttrString(pModule, (char *)"variables", dict_var);
	PyObject_SetAttrString(pModule, (char *)"stats", dict_stats_self);

	pFunc = PyObject_GetAttrString(pModule, "Init");


	if (pFunc == NULL)
	{
		Com_Printf("Python - Loading %s failed it has no Init function\n", file);
		return;
	}
	else
	{
		Com_Printf("Python - Functions in %s loaded\n", file);
	}
	PyObject_CallObject(pFunc, NULL);
	Py_DECREF(pFunc);

}

struct PyFunctions_s *Python_GetObjectByName(char *name)
{
	struct PyFunctions_s *temp;
	int i = 0;

	temp = Python_Functions;

	while (temp)
	{
		if (!strcmp(PyModule_GetName(temp->function), name))
			return temp;
		temp = temp->next;
	}
	return NULL;

}
void Python_Init(void);
void Python_Quit(void);
void Python_Reload(char *module)
{

	Python_Quit();
	Python_Init();

}

void Python_Call(char *module, char *func_name, PyObject * arguments)
{
	PyObject *mod, *func;
	struct PyFunctions_s *t;

	t = Python_GetObjectByName(module);

	if (t == NULL)
	{
		//Com_Printf("Python -CALL - No module with the name %s\n",module);
		return;
	}
	mod = t->function;
	func = PyObject_GetAttrString(mod, func_name);

	if (func == NULL)
	{
		Com_Printf("Python - Calling %s in module %s failed\n", func_name, module);
		return;
	}
	else
	{
		//Com_Printf("Python - Calling %s in module %s success !\n", function,module);
	}
	PyObject_CallObject(func, arguments);
	Py_DECREF(func);

}


void Particle_Reload_From_Console(void)
{
	Python_Reload(Cmd_Argv(1));
}

void Python_Call_From_Console(void)
{

	Python_Call(Cmd_Argv(1), Cmd_Argv(2), NULL);

}

void Python_Reinit_From_Console(void)
{
	//Py_Finalize();
	python_initialized = 0;
	Python_Init();


}

void Python_Init(void)
{
	char newpath[4096];
	PyObject *dict1, *temp1, *temp2, *temp3;
	struct PyFunctions_s *mod;


	Py_Initialize();


	if (!python_initialized)
	{
		strcpy(newpath, Py_GetPath());
		strcat(newpath, ":./python:./");
		PySys_SetPath(newpath);
		Cmd_AddCommand("python_reload_module", Particle_Reload_From_Console);
		Cmd_AddCommand("python_call", Python_Call_From_Console);
		Cmd_AddCommand("python_reinit", Python_Reinit_From_Console);
	}


	if (Py_IsInitialized())
	{
		Com_Printf("Python - Initialized\n");
		Com_Printf("Python - Version %s\n", Py_GetVersion());
		dict_var = PyDict_New();
		PyDict_SetItemString(dict_var, "variables", PyEval_GetBuiltins());
		dict_stats_self = PyDict_New();
		PyDict_SetItemString(dict_var, "stats", PyEval_GetBuiltins());
		python_initialized = true;
		Python_Hooks();
	}
	else
	{
		Com_Printf("Python - Initialization failed\n");
		python_initialized = false;
		return;
	}

	Python_Load("main");




}

void Python_Quit(void)
{
	struct PyFunctions_s *temp, *old;

	temp = Python_Functions;
	while (temp)
	{
		Py_DECREF(temp->function);
		old = temp;
		temp = temp->next;
		free(old);

	}
	Python_Functions = NULL;
	Py_Finalize();
	Com_Printf("Closing Python interpreter\n");
}

void Python_Frame(void)
{
	PyObject *pArgs, *pValue;
	int i = 0;

	pArgs = PyTuple_New(1);
	pValue = Py_BuildValue("d", Sys_DoubleTime());
	PyTuple_SetItem(pArgs, i++, pValue);
	Python_Call("main", "frame", pArgs);
	//Com_Printf("%f\n",PyDict_Size(dict_var));

}






void Python_RegisterVariable(cvar_t * var)
{
	if (!python_initialized)
		return;
	PyDict_SetItemString(dict_var, var->name, PyString_FromString(var->string));


}
