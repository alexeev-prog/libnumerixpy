# Как увеличить скорость python-программы благодаря C-расширениям.
Во время разработки ПО мы сталкиваемся с выбором между удобством языка и его производительностью. Python завоевал популярность благодаря простоте и изящности, но когда дело доходит до низкоуровневых действий или махинаций, требующие производительность и быстроту, на помощь приходит C.

Мы будем изучать именно интеграцию расширений во времени сборки, а не просто загрузка библиотек через ctypes.

В этой статье я хочу рассказать о том, как интегрировать C-расширения с использованием библиотеки Python.h. Я также расскажу как создать свою python-библиотеку с C-расширениями. Также мы исследуем, как устроен Python - например, вспомним, что все является объектами. Я буду использовать poetry как менеджер рабочего окружения.

Все будет создаваться на примере моей небольшой библиотеки для различных алгоритмов и вычислений. В конце я проведу анализ pure-python алгоритмов, нашей библиотеки и pure-c алгоритмов: скорость выполнения, распространяемость, минусы и плюсы, количество кода.

Не буду тянуть, начнем!

---

Итак, допустим, вы хотите реализовать какой-либо функционал в вашем проекте. Но понимаете, что чистый python слишком медленный или слишком высокоуровневый для решения вашей задачи. Поэтому можно создавать C-расширения, в которых будет реализован код, который критичен к скорости выполнения.

 > C-расширения доступны только для cpython - эталонной реализации python.

Также python позволяет создавать C-расширения, выполняющие операции без GIL - Global Interpreter Lock.

![](https://habrastorage.org/r/w1560/getpro/habr/post_images/186/54c/962/18654c9629e7361c85336380dd7c466d.png)

 > GIL накладывает некоторые ограничения на потоки, а именно что нельзя использовать несколько процессоров одновремено. Он представляет собой мьютекс, который блокирует доступ к объекту Python interpreter в многопоточных средах, разрешая выполнять лишь одну инструкцию за раз. Этот механизм, хоть и заботится о целостности данных, может тормозить работу программы.

Но также можно и другие задачи переложить на C-расширения, такие как:

 + Вычисления с высокой интенсивностью: алгоритмы, требующие большое количество тяжелых математических операций. Например, быстрое преобразование Фурье (которое мы рассмотрим в этой статье) или сложные операции с матрицами.
 + Низкоуровневое программирование: прямой доступ к памяти, системные вызовы, низкоуровневая работа с сокетами и устройствами ввода/вывода.
 + Реализация функционала, который часто используется и является узким местом.
 + Непосредственная работа с C-библиотеками.
 + Алгоритмы компрессии и декомпрессии данных.

 > Важно понимать, что иногда не стоит все писать на C, иногда можно обойтись обычными оптимизациями и профилированием.

# Настройка окружения
Итак, как обычно начинается создание проектов на python? Банально создание виртуального окружения

```bash
python3 -m venv venv
source venv/bin/activate
```

Но в этом проекте я решил отойти от такого способа, и использовать вместо этого систему правлению проектами Poetry. Poetry — это инструмент для управления зависимостями и сборкой пакетов в Python. А также при помощи Poetry очень легко опубликовать свою библиотеку на PyPi!

В Poetry представлен полный набор инструментов, которые могут понадобиться для детерминированного управления проектами на Python. В том числе, сборка пакетов, поддержка разных версий языка, тестирование и развертывание проектов.

Все началось с того, что создателю Poetry Себастьену Юстасу потребовался единый инструмент для управления проектами от начала до конца, надежный и интуитивно понятный, который бы мог использоваться и в рамках сообщества. Одного лишь менеджера зависимостей было недостаточно, чтобы управлять запуском тестов, процессом развертывания и всем созависимым окружением. Этот функционал находится за гранью возможностей обычных пакетных менеджеров, таких как Pip или Conda. Так появился Python Poetry.

Установить poetry можно через pipx: `pipx install poetry` и через pip: `pip install poetry --break-system-requirements`. Это установит poetry глобально во всю систему.

Давайте инициализируем проект в домашней директории:

```bash
poetry init
```

Но для того, чтобы мы могли писать расширения на C, нам нужно будет получить доступ к C API Python (к заголовочному файлу python.h). Для этого надо установить пакет `python-dev` или `python3-dev`.

Но это еще не все. Так как мы используем расширения на компилируемом языке C, надо будет создать скрипт для сборки (build.py):

```python
"""Build script."""

from setuptools import Extension
from setuptools.command.build_ext import build_ext

extensions = [
	Extension("libnumerixpy.base", sources=["ext/src/lnpy_base.c"]),
	Extension("libnumerixpy.math.basemath", sources=['ext/src/libbasemath.c', "ext/src/lnpy_basemath.c"], include_dirs=['ext/src']),
]


class BuildFailed(Exception):
	pass


class ExtBuilder(build_ext):
	def run(self):
		try:
			build_ext.run(self)
		except Exception as ex:
			print(f'[run] Error: {ex}')

	def build_extension(self, ext):
		try:
			build_ext.build_extension(self, ext)
		except Exception as ex:
			print(f'[build] Error: {ex}')


def build(setup_kwargs):
	setup_kwargs.update(
		{"ext_modules": extensions, "cmdclass": {"build_ext": ExtBuilder}}
	)
```

Мой проект называется libnumerixpy, я буду реализовывать некоторые функции для математических расчетов. Давайте разберем код:

```python
extensions = [
	Extension("libnumerixpy.base", sources=["ext/src/lnpy_base.c"]),
	Extension("libnumerixpy.math.basemath", sources=['ext/src/libbasemath.c', "ext/src/lnpy_basemath.c"], include_dirs=['ext/src']),
]
```

Это список расширений, названий модулей и путей до файлов исходного кода, а также директории для включения (нужно чтобы он увидел libbasemath.h, который мы напишем в будущем).

```
ext
└── src
    ├── libbasemath.c
    ├── libbasemath.h
    ├── lnpy_base.c
    └── lnpy_basemath.c
```

Мы их рассмотрим позже. Но перед тем как изучить Python C API, давайте подключим наш скрипт сборки в pyproject.toml:

```toml
[build-system]
requires = ["poetry-core", 'setuptools']
build-backend = "poetry.core.masonry.api"
```

 > pyproject.toml - это файл нашего проекта, где находится мета-информация, зависимости и правила сборки. Он автоматически создается, если использовать poetry.

# Python C-API
Итак, для того чтобы писать расширения на Python нам нужно изучить Python/C API.

 > Python/C API - это интерфейс прикладного программирования на Python, который предоставляет разработчикам доступ к интерпретатору Python.

Для написания C-кода для Python есть [PEP7](https://docs.python.org/pep-0007).

Вот его основные положения:

1. Версия стандарта C - C11 (python >=3.11, а в python 3.6-3.10 используется C89/C99)
2. Не использовать расширения для конкретных компиляторов.
3. Все объявления и определения функций должны использовать полные прототипы (то есть указывать типы всех аргументов).
4. Нет предупреждений в процессе компиляции (основных компиляторов).
5. Используйте отступы в 4 пробела (без табуляций). Как по мне, эта часть редко соблюдается.
6. Ни одна строка не должна быть длинее 79 символов.
7. Стиль определения функции (тип функции в первой строке, имя и аргументы во второй, скобки в третьей, пуста строка после объявления локальных переменных):

```c
static PyObject
*calculate_discriminant(PyObject *self, PyObject *args) {
	double a, b, c;

	if (!PyArg_ParseTuple(args, "ddd", &a, &b, &c)) {
		return NULL;
	}

	double discriminant = b * b - 4 * a * c;

	return Py_BuildValue("d", discriminant);
}
```

А сам пример кода выглядит следующим образом:

```c
#define PY_SSIZE_T_CLEAN
// #define Py_GIL_DISABLED // Подключать только если включена экспериментальная функция отключения GIL в Python 3.13
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

static PyMethodDef LNPYMethods[] = { { "exec_shell_command", lnpy_exec_system, METH_VARARGS,
									   "Execute a shell command." },
									 { NULL, NULL, 0, NULL } };

static struct PyModuleDef lnpy_base = { PyModuleDef_HEAD_INIT, "base", NULL, -1, LNPYMethods };

PyMODINIT_FUNC PyInit_base(void) { return PyModule_Create(&lnpy_base); }
```

## Макросы
В Python/C API есть несколько полезных макросов. Рассмотрим несколько из них:

 + PyMODINIT_FUNC

Она нужна для того чтобы задать функцию инициализации модуля (PyInit). Функция должна возвращать PyObject.

Функция инициализации должна иметь имя в формате PyInit_name, где *name* — это имя модуля, и функция должна быть единственным нестатическим элементом.


Примеры использования:

```c
static struct PyModuleDef lnpy_base = { PyModuleDef_HEAD_INIT, "base", NULL, -1, LNPYMethods };

PyMODINIT_FUNC PyInit_base(void) { return PyModule_Create(&lnpy_base); }
```

Из [официальной документации](https://docs.python.org/3/c-api/intro.html#useful-macros):

```c
static struct PyModuleDef spam_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "spam",
    ...
};

PyMODINIT_FUNC
PyInit_spam(void)
{
    return PyModule_Create(&spam_module);
}
```

 + Py_ABS(x) - возвращает абсолютное значение x.
 + Py_MAX(x, y) - максимальное значение среди x и y.
 + Py_MIN(x, y) - минимальное значение среди x и y
 + Py_STRINGIFY(x) - превращает x в строку (Py_STRINGIFY(123) > "123")
 + PyDoc_STRVAR(name, str) - Создает переменную с именем name, которое может быть использовано в строках документации.

```c
PyDoc_STRVAR(pop_doc, "Remove and return the rightmost element.");

static PyMethodDef deque_methods[] = {
    // ...
    {"pop", (PyCFunction)deque_pop, METH_NOARGS, pop_doc},
    // ...
}
```

 + PyDoc_STR(str) - Создает строку документации.

```c
static PyMethodDef pysqlite_row_methods[] = {
    {"keys", (PyCFunction)pysqlite_row_keys, METH_NOARGS,
        PyDoc_STR("Returns the keys of the row.")},
    {NULL, NULL}
};
```

## Исключения
Программисту Python приходится иметь дело с исключениями только в том случае, если требуется особая обработка ошибок; необработанные исключения автоматически передаются вызывающей стороне, затем вызывающей стороне вызывающей стороны и т. д., пока не достигнут интерпретатора верхнего уровня, где они сообщаются пользователю вместе с трассировкой стека.

Однако для программистов на языке C проверка ошибок всегда должна быть явной. Все функции в API Python/C могут вызывать исключения, если в документации функции явно не указано иное. В общем, если функция сталкивается с ошибкой, она устанавливает исключение, отбрасывает все ссылки на объекты, которыми она владеет, и возвращает индикатор ошибки. Если не указано иное, этот индикатор может быть NULL или -1, в зависимости от типа возвращаемого значения функции. Несколько функций возвращают логический результат true/false, при этом false указывает на ошибку. Очень немногие функции не возвращают явного индикатора ошибки или имеют неоднозначное возвращаемое значение и требуют явного тестирования на наличие ошибок с помощью PyErr_Occurred(). Эти исключения всегда явно документируются.

Пример:

```python
def incr_item(dict, key):
    try:
        item = dict[key]
    except KeyError:
        item = 0
    dict[key] = item + 1
```

```c
int
incr_item(PyObject *dict, PyObject *key)
{
    PyObject *item = NULL, *const_one = NULL, *incremented_item = NULL;
    int rv = -1;

    item = PyObject_GetItem(dict, key);
    if (item == NULL) {
        /* Обработка KeyError */
        if (!PyErr_ExceptionMatches(PyExc_KeyError))
            goto error;

        /* Очистка ошибки и использование нуля: */
        PyErr_Clear();
        item = PyLong_FromLong(0L);
        if (item == NULL)
            goto error;
    }
    const_one = PyLong_FromLong(1L);
    if (const_one == NULL)
        goto error;

    incremented_item = PyNumber_Add(item, const_one);
    if (incremented_item == NULL)
        goto error;

    if (PyObject_SetItem(dict, key, incremented_item) < 0)
        goto error;
    rv = 0; /* Успех */
    /* Конец и очистка кода */

 error:
    /* Код очистки */

    /* Используйте Py_XDECREF(), чтобы игнорировать ссылки NULL. */
    Py_XDECREF(item);
    Py_XDECREF(const_one);
    Py_XDECREF(incremented_item);

    return rv;
```

Исключения Python сильно отличаются от исключений C/C++. Если вы хотите вызвать исключения Python из вашего модуля расширения C, то можете использовать Python API для этого. Вот некоторые функции, предоставляемые Python API для вызова исключений:

 + `PyErr_SetString(PyObject *type, const char *message)` - Принимает два аргумента: аргумент типа PyObject, указывающий тип исключения, и настраиваемое сообщение для отображения пользователю.
 + `PyErr_Format(PyObject *type, const char *format)` - Принимает два аргумента: аргумент типа PyObject, указывающий тип исключения, и отформатированное настраиваемое сообщение для отображения пользователю
 + `PyErr_SetObject(PyObject *type, PyObject *value)` - Принимает два аргумента, оба типа PyObject: первый указывает тип исключения, а второй устанавливает произвольный объект Python в качестве значения исключения.

Хотя вы не можете вызывать исключения в C, Python API позволит вам вызывать исключения из вашего модуля расширения Python C. Давайте проверим эту функциональность, добавив PyErr_SetString():

```c
static PyObject *method_fputs(PyObject *self, PyObject *args) {
    char *str, *filename = NULL;
    int bytes_copied = -1;

    /* Parse arguments */
    if(!PyArg_ParseTuple(args, "ss", &str, &fd)) 
        return NULL;
    
    if (strlen(str) < 10) {
        PyErr_SetString(PyExc_ValueError, "String length must be greater than 10");
        return NULL;
    }

    fp = fopen(filename, "w");
    bytes_copied = fputs(str, fp);
    fclose(fp);

    return PyLong_FromLong(bytes_copied);
}
```

Этот код вызовет исключение, если мы попытаемся записать в файл строку длиной меньше 10 символов.

### Пользовательские исключения
Вы также можете вызывать пользовательские исключения в вашем модуле расширения Python:

```c
static PyObject *StringTooShortError = NULL;

PyMODINIT_FUNC PyInit_fputs(void) {
    /* Assign module value */
    PyObject *module = PyModule_Create(&fputsmodule);

    /* Initialize new exception object */
    StringTooShortError = PyErr_NewException("fputs.StringTooShortError", NULL, NULL);

    /* Add exception object to your module */
    PyModule_AddObject(module, "StringTooShortError", StringTooShortError);

    return module;
}
```

Как и прежде, вы начинаете с создания объекта модуля. Затем вы создаете новый объект исключения, используя PyErr_NewException. Это принимает строку вида module.classname как имя класса исключения, который вы хотите создать. Выберите что-то описательное, чтобы пользователю было легче интерпретировать то, что на самом деле пошло не так.

Затем вы добавляете это к своему объекту модуля, используя PyModule_AddObject. Он принимает объект вашего модуля, имя добавляемого нового объекта и сам объект пользовательского исключения в качестве аргументов. Наконец, вы возвращаете объект вашего модуля.

Теперь, когда вы определили настраиваемое исключение для вашего модуля, вам нужно обновить method_fputs(), чтобы оно вызывало соответствующее исключение:

```c
static PyObject *method_fputs(PyObject *self, PyObject *args) {
    char *str, *filename = NULL;
    int bytes_copied = -1;

    /* Parse arguments */
    if(!PyArg_ParseTuple(args, "ss", &str, &fd)) 
        return NULL;
    
    if (strlen(str) < 10) {
        /* Кастомное исключение */
        PyErr_SetString(StringTooShortError, "String length must be greater than 10");
        return NULL;
    }

    fp = fopen(filename, "w");
    bytes_copied = fputs(str, fp);
    fclose(fp);

    return PyLong_FromLong(bytes_copied);
}
```

## Определение констант
Вы можете задавать константы, нужные вам, сразу в C-коде. Для integer-констант можно использовать PyModule_AddIntConstant:

```c
PyMODINIT_FUNC PyInit_module(void) {
    PyObject *module = PyModule_Create(<ваш модуль>);

    /* Добавляем целочисленную константу */
    PyModule_AddIntConstant(module, "INT_PI", 3);

    #define INT_PI 256

    PyModule_AddIntMacro(module, INT_PI);

    return module;
}
```

Эта функция Python API принимает следующие аргументы:

1. Экземпляр вашего модуля.
2. Наименование константы.
3. Значение константы.

## Почему в Python все - объекты?
В коде уже не раз упоминался некий универсальный PyObject. Все типы объектов являются расширениями этого типа. Это тип, который содержит информацию, необходимую Python для обработки указателя на объект как объекта.

В Python почти все является объектом, будь то число, функция или модуль. Python использует чистую объектную модель, где классы являются экземплярами метакласса type. При этом термины «тип» и «класс» являются синонимами, а type — это единственный класс, который является экземпляром самого себя.

PyObject - это структура объектов, которую вы используете для определения типов объектов для Python. Все объекты Python имеют небольшое количество полей, определенных с помощью структуры PyObject. Все остальные типы объектов являются расширениями этого типа.

PyObject говорит интерпретатору Python обрабатывать указатель на объект как объект. Например, установка типа возврата вышеуказанной функции как PyObject определяет общие поля, которые требуются интерпретатору Python для распознавания его как допустимого типа Python.

## Реализация функций и методов 
Для создания функций и методов на C/Python API используется тип PyCFunction. Тип функций, используемых для реализации большинства вызываемых объектов Python в C. Функции этого типа принимают два параметра PyObject * и возвращают одно такое значение.

```python
PyObject *PyCFunction(PyObject *self,
                      PyObject *args);
```

Также существуют следующие макросы-вызовы:

1. **METH_VARARGS** 
Это типичное соглашение о вызовах, где методы имеют тип PyCFunction. Функция ожидает два значения PyObject* . Первое из них — это объект self для методов; для функций модуля — это объект module. Второй параметр (часто называемый args) — это объект кортежа, представляющий все аргументы. Этот параметр обычно обрабатывается с помощью PyArg_ParseTuple()или PyArg_UnpackTuple().

2. **METH_KEYWORDS**
Может использоваться только в определенных комбинациях с другими флагами: [METH_VARARGS | METH_KEYWORDS](https://docs.python.org/3/c-api/structures.html#meth-varargs-meth-keywords) , [METH_FASTCALL | METH_KEYWORDS](https://docs.python.org/3/c-api/structures.html#meth-fastcall-meth-keywords) и [METH_METHOD | METH_FASTCALL | METH_KEYWORDS](https://docs.python.org/3/c-api/structures.html#meth-method-meth-fastcall-meth-keywords).

Больше информации вы можете посмотреть в [официальной документации](https://docs.python.org/3/c-api/structures.html#c.PyCFunctionWithKeywords).

## Практический пример
Давайте создадим несколько функций, чтобы на примере разобраться в Python/C API.

 > Вверху статьи находится гайд по настройке окружения.

Для начала создадим небольшой pure-c файл `libbasemath.c` в директории ext/src:

```c
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
```

В нем вы можете заметить вычисление дискриминанта квадратного уравнения и вычисление факториала.

А также создадим заголовочный файл libbasemath.h:

```c
#ifndef LIBBASEMATH_H
#define LIBBASEMATH_H

double calculate_discriminant(double a, double b, double c);
unsigned long cfactorial_sum(char num_chars[]);
unsigned long ifactorial_sum(long nums[], int size);
unsigned long factorial(long n);

#endif // LIBBASEMATH_H
```

Разберем код:

 + функция `calculate_discriminant` высчитывает дискриминант квадратного уравнения по формуле D = b^2 * 4ac.
 + функция `cfactorial_sum` высчитывай факториал суммы, читая из строки.
 + функция `ifactorial_sum` высчитывает факториал суммы, читая из списка.
 + функция `factorial` высчитывает факториал (вспомогательная функция).

Теперь займемся файлом `lnpy_basemath.c`, который будет содержать Python/C обертки для функций:

```c
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
```

В первых строках мы задаем макросы и включаем нужные заголовочные файлы:

```c
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <stdio.h>
#include <libbasemath.h>
```

Также мы создаем статичные функции-обертки, который возвращают PyObject: Py_calculate_discriminant, cFactorial_sum, iFactorial_sum.

Рассмотрим несколько интересных моментов:

 + PyBuildValue позволяет создать значение из строки: 

```c
Py_BuildValue("s", "A") // "А"
Py_BuildValue("i", 10) // 10
Py_BuildValue("(iii)", 1, 2, 3) //	(1, 2, 3)
Py_BuildValue("{si,si}", "a", 4, "b", 9) //	{"а": 4, "б": 9}
Py_BuildValue("") // None
```

 + PyObject_Length нужен для получения размера массива
 + PyList_GetItem - позволяет получить элемент массива по его индексу.
 + PyLong_AsLong - нужен для преобразования Python-представления long в общий тип данных C long.

В самом конце уже идет работа над модулем: через тип данных PyMethodDef мы создаем массив с n+1 элементами, где n - количество функций.

```c
static PyMethodDef LNPYMethods[] = { { "calculate_discriminant", Py_calculate_discriminant, METH_VARARGS,
									   "Calculate the discriminant by formula: D = b^2 * 4ac" },
									 { "ifactorial_sum", iFactorial_sum, METH_VARARGS,
									   "Calculate the iFactorial sum (from list of ints)" },
									 { "cfactorial_sum", cFactorial_sum, METH_VARARGS,
									   "Calculate the cFactorial sum (from digits in string of numbers)" },
									 { NULL, NULL, 0, NULL } };
```

Чтобы вызвать методы, определенные в вашем модуле, вам нужно сначала сообщить о них интерпретатору Python. Для этого вы можете использовать PyMethodDef. Это структура с 4 членами, представляющими один метод в вашем модуле.

В идеале в вашем модуле расширения Python C должно быть несколько методов, которые вы хотите вызывать из интерпретатора Python. Вот почему вам нужно определить массив структур PyMethodDef.

METH_VARARGS - это флаг, который сообщает интерпретатору, что функция будет принимать два аргумента типа PyObject.

На примере calculate_discriminant:

 + `"calculate_discriminant"` - это название функции, которое будет использоваться в питоне.
 + Py_calculate_discriminant - сама функция.
 + METH_VARARGS мы рассмотрели выше
 + "Calculate the discriminant by formula: D = b^2 * 4ac" - это docstring, строка документации.

Последние строки - это инициализация модуля:

```c
static struct PyModuleDef lnpy_basemath = { PyModuleDef_HEAD_INIT, "math", "Libnumerixpy - BaseMath", -1, LNPYMethods };

PyMODINIT_FUNC PyInit_basemath(void) { return PyModule_Create(&lnpy_basemath); }
```

Так же, как PyMethodDef содержит информацию о методах в вашем модуле расширения Python, структура PyModuleDef содержит информацию о самом модуле. Это не массив структур, а единственная структура, которая используется для определения модуля.

Эти строки позволют нам использовать конструкцию `from libnumerixpy.math.basemath import calculate_discriminant, cfactorial_sum, ifactorial_sum`.

Первая строчка - это структура типа PyModuleDef где мы задаем название, докстринг и список методов. А последняя отвечает за финальное создание и инициализацию модуля.

---

Давайте рассмотрим другой пример, попроще:

```c
#define PY_SSIZE_T_CLEAN
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
```

Здесь абсолютно также, но функция одна - для выполнения системной команды (конструкция импорта `from libnumerixpy.base import lnpy_exec_system`).

---

Вы могли заметить такие функции из Python/C API, например PyLong_FromLong, PyArg_ParseTuple и другие. Сейчас я рассмотрю их подробнее.

PyArg_ParseTuple() анализирует аргументы, которые вы получите от вашей программы Python, в локальные переменные:

```c
	const char *command;

	if (!PyArg_ParseTuple(args, "s", &command)) {
		return NULL;
	}
```

Он принимает на вход массив аргументов, типы данных и указатель на переменные. Сами типы данных отображаются также как в printf. Спецификации форматов вы можете [посмотреть в официальной документации](https://docs.python.org/3/c-api/arg.html).

Перейдем к PyLong_FromLong:

PyLong_FromLong() возвращает PyLongObject, который представляет целочисленный объект в Python. Вы можете найти его в самом конце вашего C-кода:

```c
	return PyLong_FromLong(sts);
```

# Бенчмарк
Давайте сравним скорость выполнения pure-python функций и C-расширений.

Давайте напишем наши функции суммы факториала из списка и строки, а также нахождения дискриминанта на чистом питоне:

```python


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
```

И теперь создадим код бенчмаркинга:

```python
from purepython import pure_calculate_discriminant, pure_cfactorial_sum, pure_ifactorial_sum
from libnumerixpy.math import calculate_discriminant, cfactorial_sum, ifactorial_sum
import timeit
from functools import wraps


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
    assert pure_ifactorial_sum([1,2,3,4,5]) == 153


@timing
def c_extension():
    d = calculate_discriminant(1.0, -3.0, 1.0)
    assert d == 5.0
    assert cfactorial_sum("12345") == 153
    assert ifactorial_sum([1,2,3,4,5]) == 153


def ppure_python():
    d = pure_calculate_discriminant(1.0, -3.0, 1.0)
    assert d == 5.0
    assert pure_cfactorial_sum("12345") == 153
    assert pure_ifactorial_sum([1,2,3,4,5]) == 153


def pc_extension():
    d = calculate_discriminant(1.0, -3.0, 1.0)
    assert d == 5.0
    assert cfactorial_sum("12345") == 153
    assert ifactorial_sum([1,2,3,4,5]) == 153


_, ellapsed_time = pure_python()
_, ellapsed_time2 = c_extension()

print(f'[PURE PYTHON] Elapsed time: {ellapsed_time}')
print(f'[C EXTENSION] Elapsed time: {ellapsed_time2}')

execution_time = timeit.timeit(ppure_python, number=1000)  # указывается количество запусков функции
print("Среднее время выполнения функции pure_python:", execution_time)

execution_time2 = timeit.timeit(pc_extension, number=1000)  # указывается количество запусков функции
print("Среднее время выполнения функции c_extension:", execution_time2)
```

У меня получился такой вывод:

```
[PURE PYTHON] Elapsed time: 0.0001080130004993407
[C EXTENSION] Elapsed time: 2.0455998310353607e-05
Среднее время выполнения функции pure_python: 0.027656054000544827
Среднее время выполнения функции c_extension: 0.0061371510000753915
```

Если переводить из экспонциального вычисления, то скорость функции с использованием C-расширений равна 0.000020455998310353607 секунд. 0.00002045599831035 меньше 0.00010801300049934 в 5 раз. Прирост скорости в 5 раз!

А во втором случае также: 0.00613715100007539 меньше 0.02765605400054482 в 5 раз.

Получаем итоговый прирост в 5 раз! Невероятно!

Если увеличить количество запусков функции до 100 000, то уже:

```
Среднее время выполнения функции pure_python: 2.7358566370003246
Среднее время выполнения функции c_extension: 0.5545017490003374
```

А как насчет миллиона?

```
Среднее время выполнения функции pure_python: 39.040380934000495
Среднее время выполнения функции c_extension: 5.6175804949998565
```

Здесь уже прирост аж в 7 раз!

Для чистоты эксперимента давайте вычислим среднее время если 100 раз запустить код по 10 тысяч раз:

```python
sum_1 = []
sum_2 = []

average_1 = 0
average_2 = 0

for i in range(100):
    execution_time = timeit.timeit(ppure_python, number=10000)  # указывается количество запусков функции
    print("Среднее время выполнения функции pure_python:", execution_time)

    sum_1.append(execution_time)

    execution_time2 = timeit.timeit(pc_extension, number=10000)  # указывается количество запусков функции
    print("Среднее время выполнения функции c_extension:", execution_time2)

    sum_2.append(execution_time2)

average_1 = sum(sum_1) / len(sum_1)
average_2 = sum(sum_2) / len(sum_2)

print(f'Среднее pure_python: {average_1}')
print(f'Среднее c_extension: {average_2}')
```

```
 >>> Среднее pure_python: 0.32199167845032206
 >>> Среднее c_extension: 0.06911946617015928
```

Прирост также в 5 раз.

# Заключение
C-расширения - вещь полезная. Допустим, необходимо выполнить ряд сложных вычислений, будь то криптоалгоритм, машинное обучение или обработка больших объемов данных. Расширения С могут взять долю нагрузки Python на себя и ускорить работу приложения. 

Решили создать низкоуровневый интерфейс или поработать напрямую с памятью из Python? Расширения С к вашим услугам, учитывая, что вы знаете, как задействовать необработанные указатели. 

Задумали улучшить уже существующее, но плохо работающее приложение Python и при этом не хотите (или не можете) переписать его на другом языке? Выход есть - расширения С. 

А может вы просто убежденный сторонник оптимизации, который стремится максимально ускорить выполнение своего кода, при этом не отказываясь от высокоуровневых абстракций для работы в сети, GUI и т.д. 

В данной статье мы рассмотрели устройство Python/C API и написали несколько своих расширений. Если вы считаете, что я где-то ошибся или неправильно выразился - пишите в комментариях.

Мы наглядно поняли, насколько можно увеличить скорость программы за счет читаемости.

Гитхаб-репозиторий со всем исходным кодом и тестами доступен [по ссылке](https://github.com/alexeev-prog/libnumerixpy).

Установить мою библиотеку можно через pip:

```bash
pip3 install libnumerixpy 
```

Буду рад, если вы присоединитесь к моему небольшому [телеграм-блогу](https://t.me/hex_warehouse). Анонсы статей, новости из мира IT и полезные материалы для изучения программирования и смежных областей.

## Источники

 + [Документация Python](https://docs.python.org/3/c-api/index.html)
 + [Реализация целого типа в CPython](https://habr.com/ru/post/455114/)
 + [Реализация строкового типа в CPython](https://habr.com/ru/post/480324/)
 + [dm-fedorov/python-modules/c-api.md](https://github.com/dm-fedorov/python-modules/blob/master/c-api.md)
