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

Это список расширений, названий модулей и путей до файлов исходного кода.

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

## Почему в Python все - объекты?
В коде уже не раз упоминался некий универсальный PyObject. Все типы объектов являются расширениями этого типа. Это тип, который содержит информацию, необходимую Python для обработки указателя на объект как объекта.

В Python почти все является объектом, будь то число, функция или модуль. Python использует чистую объектную модель, где классы являются экземплярами метакласса type. При этом термины «тип» и «класс» являются синонимами, а type — это единственный класс, который является экземпляром самого себя.

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

Для начала создадим небольшой pure-c файл `basemath.c` в директории ext/src:

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

А также создадим заголовочный файл basemath.h

# Заключение
C-расширения - вещь полезная. Допустим, необходимо выполнить ряд сложных вычислений, будь то криптоалгоритм, машинное обучение или обработка больших объемов данных. Расширения С могут взять долю нагрузки Python на себя и ускорить работу приложения. 

Решили создать низкоуровневый интерфейс или поработать напрямую с памятью из Python? Расширения С к вашим услугам, учитывая, что вы знаете, как задействовать необработанные указатели. 

Задумали улучшить уже существующее, но плохо работающее приложение Python и при этом не хотите (или не можете) переписать его на другом языке? Выход есть - расширения С. 

А может вы просто убежденный сторонник оптимизации, который стремится максимально ускорить выполнение своего кода, при этом не отказываясь от высокоуровневых абстракций для работы в сети, GUI и т.д. 

В данной статье мы рассмотрели устройство Python/C API и написали несколько своих расширений. Если вы считаете, что я где-то ошибся или неправильно выразился - пишите в комментариях.

## Источники

 + [Документация Python](https://docs.python.org/3/c-api/index.html)

