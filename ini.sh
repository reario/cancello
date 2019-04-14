#!/bin/bash

echo "# cancello" >> README.md
git init
#git add README.md
git add .
git commit -am "first commit"
git remote add origin git@github.com:reario/cancello.git
git push -u origin master
