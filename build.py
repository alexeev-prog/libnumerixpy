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

