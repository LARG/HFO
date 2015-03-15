import os
import setuptools

setuptools.setup(
    name='hfo',
    version='0.1.1',
    packages=setuptools.find_packages(),
    author='Matthew Hausknecht',
    author_email='matthew.hausknecht@gmail.com',
    description='Transparent cluster execution.',
    license='MIT',
    keywords=('Robocup '
              'Half-field-offense '
              ),
    classifiers=[
        'Development Status :: 4 - Beta',
        'Intended Audience :: Science/Research',
        'License :: OSI Approved :: MIT License',
        'Operating System :: OS Independent',
        ],
    )
