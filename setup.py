from distutils.core import setup, Extension
import os.path, sys

hfo_c_lib = 'hfo/libhfo_c.so'
if not os.path.isfile(hfo_c_lib):
  print('ERROR: Unable to find required library: %s.'%(hfo_c_lib))
  sys.exit()

module1 = Extension('hfo.hfo_c_wrapper',
                    libraries = ['hfo_c'],
                    include_dirs = ['src'],
                    library_dirs = ['hfo'],
                    extra_compile_args=['-D__STDC_CONSTANT_MACROS'],
                    sources=['hfo/hfo_c_wrapper.cpp'])
setup(name = 'hfo',
      version='0.1.5',
      author='Matthew Hausknecht',
      author_email='matthew.hausknecht@gmail.com',
      description='Half Field Offense in 2D RoboCup Soccer.',
      license='MIT',
      ext_modules = [module1],
      keywords=('Robocup '
                'Half-field-offense '
              ),
      classifiers=[
        'Development Status :: 4 - Beta',
        'Intended Audience :: Science/Research',
        'License :: OSI Approved :: MIT License',
        'Operating System :: OS Independent',
      ],
      # packages=find_packages(),
      packages=['hfo'],
      package_dir={'hfo': 'hfo'},
      package_data={'hfo': ['libhfo_c.so']})
