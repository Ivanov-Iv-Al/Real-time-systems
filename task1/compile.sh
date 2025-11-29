#!/bin/bash

echo "Компиляция доступных программ..."

# Проверяем наличие файлов и компилируем только то, что есть
if [ -f "mutex.c" ]; then
    gcc -o mutex mutex.c -lpthread
    echo "mutex.c скомпилирован -> mutex"
else
    echo "⚠️  mutex.c не найден"
fi

if [ -f "condvar.c" ]; then
    gcc -o condvar condvar.c -lpthread
    echo "condvar.c скомпилирован -> condvar"
else
    echo "⚠️  condvar.c не найден"
fi

if [ -f "scenario_1.c" ] && [ -f "working.c" ] && [ -f "working.h" ]; then
    gcc -o scenario_1 scenario_1.c working.c -lpthread
    echo "scenario_1.c скомпилирован -> scenario_1"
else
    echo "⚠️  scenario_1.c, working.c или working.h не найдены"
fi

if [ -f "scenario_2.c" ] && [ -f "working.c" ] && [ -f "working.h" ]; then
    gcc -o scenario_2 scenario_2.c working.c -lpthread
    echo "scenario_2.c скомпилирован -> scenario_2"
else
    echo "⚠️  scenario_2.c, working.c или working.h не найдены"
fi

if [ -f "intsimple.c" ]; then
    gcc -o intsimple intsimple.c
    echo "intsimple.c скомпилирован -> intsimple"
else
    echo "⚠️  intsimple.c не найден"
fi

if [ -f "resmgr.c" ]; then
    gcc -o resmgr resmgr.c -lpthread
    echo "resmgr.c скомпилирован -> resmgr"
else
    echo "⚠️  resmgr.c не найден"
fi

if [ -f "client.c" ]; then
    gcc -o client client.c
    echo "client.c скомпилирован -> client"
else
    echo "⚠️  client.c не найден"
fi

echo ""
echo "Компиляция завершена!"
echo "Доступные исполняемые файлы:"
ls -la | grep -E '(mutex|condvar|scenario_|intsimple|resmgr|client)' | grep -vE '\.(c|h)'