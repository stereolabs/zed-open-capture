#!/bin/bash -e

echo Generation documentation...
cd api_doc
doxygen
cd ..
echo ... documentation ready.
