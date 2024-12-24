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
	Extension("libnumerixpy.cmath", sources=["ext/src/lnpy_cmath.c"]),
]


class BuildFailed(Exception):
	pass


class ExtBuilder(build_ext):
	def run(self):
		try:
			build_ext.run(self)
		except Exception as ex:
			print(ex)

	def build_extension(self, ext):
		try:
			build_ext.build_extension(self, ext)
		except Exception as ex:
			print(ex)


def build(setup_kwargs):
	setup_kwargs.update(
		{"ext_modules": extensions, "cmdclass": {"build_ext": ExtBuilder}}
	)


```

Мой проект называется gnuxlinux, я буду реализовывать некоторые утилиты из GNU Core Utils и не только. Давайте разберем код:

```python
extensions = [
	Extension("libnumerixpy.base", sources=["ext/src/lnpy_base.c"]),
	Extension("libnumerixpy.cmath", sources=["ext/src/lnpy_cmath.c"]),
]
```

Это список расширений, названий модулей и путей до файлов исходного кода.

```
ext
└── src
    ├── lnpy_base.c
    └── lnpy_cmath.c
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
// #define Py_GIL_DISABLED // Подключать только если включена экспериментальная функция отключения GIL
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




# Заключение
В данной статье мы рассмотрели устройство Python/C API и написали несколько своих расширений. Если вы считаете, что я где-то ошибся или неправильно выразился - пишите в комментариях.

## Источники

 + [Документация Python](https://docs.python.org/3/c-api/index.html)
