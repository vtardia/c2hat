#!/bin/bash

# Copyright (C) 2020-2022 Vito Tardia <https://vito.tardia.me>
#
# This file is part of C2Hat
#
# C2Hat is a simple client/server TCP chat written in C
#
# C2Hat is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# C2Hat is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with C2Hat. If not, see <https://www.gnu.org/licenses/>.


VALGRIND=$(which valgrind)

if [ -n "$VALGRIND" ]; then
    TEST="$VALGRIND --leak-check=full --show-leak-kinds=all --track-origins=yes bin/test/pid"
else
    TEST="bin/test/pid"
fi

# Test that cannot create PID without permissions
$TEST init /var/run/c2hat-test.pid
if [[ $? == 0 ]]; then
    echo "Expected failure, got success instead"
    exit 1
fi
echo -n "."

# Test that new PID is created, should succeed
$TEST init /tmp/c2hat-test.pid
if [[ $? != 0 ]]; then
    echo "Unable to init PID file"
    exit 1
fi
echo -n "."

# Test that new PID is created, should fail
$TEST init /tmp/c2hat-test.pid
if [[ $? == 0 ]]; then
    echo "Expected failure, got success instead"
    exit 1
fi
echo -n "."

# Test failure when loading an unreachable file
$TEST load /tmp/unknown.pid
if [[ $? == 0 ]]; then
    echo "Expected failure, got success instead"
    exit 1
fi
echo -n "."

# Test that it can load an existent PID file
$TEST load /tmp/c2hat-test.pid
if [[ $? != 0 ]]; then
    echo "Unable to load PID file"
    exit 1
fi
echo -n "."

$TEST check /tmp/c2hat-test.pid
if [[ $? != 0 ]]; then
    echo "PID file does not exist"
    exit 1
fi
echo -n "."

$TEST check /tmp/unknown.pid >> /dev/null
if [[ $? == 0 ]]; then
    echo "PID file should not exist"
    exit 1
fi
echo -n "."

rm /tmp/c2hat-test.pid
echo

exit 0
