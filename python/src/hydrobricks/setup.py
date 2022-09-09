#!/usr/bin/env python
import setuptools

setuptools.setup(
    name="hydrobricks",
    version="0.0.2",
    author="Pascal Horton",
    author_email="pascal.horton@giub.unibe.ch",
    package_dir={"hydrobricks": "hydrobricks"},
    packages=setuptools.find_packages(where="hydrobricks"),
    entry_points={"console_scripts": ["hydrobricks=hydrobricks.__main__:main"]},
    classifiers=[
        "Programming Language :: Python :: 3",
        "Framework :: Pytest"
    ],
    ext_modules=[
        setuptools.Extension(
            name='fake.module',
            sources=[]
        )
    ]
)
