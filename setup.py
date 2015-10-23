import os
import setuptools

setuptools.setup(
    name='hfo',
    version='0.1.2',
    packages=setuptools.find_packages(),
    author='Matthew Hausknecht',
    author_email='matthew.hausknecht@gmail.com',
    description='Half Field Offense in 2D RoboCup Soccer.',
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
