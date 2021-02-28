#!/bin/sh

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
echo ".\c"

# Test that new PID is created, should succeed
$TEST init /tmp/c2hat-test.pid
if [[ $? != 0 ]]; then
    echo "Unable to init PID file"
    exit 1
fi
echo ".\c"

# Test that new PID is created, should fail
$TEST init /tmp/c2hat-test.pid
if [[ $? == 0 ]]; then
    echo "Expected failure, got success instead"
    exit 1
fi
echo ".\c"

# Test failure when loading an unreachable file
$TEST load /tmp/unknown.pid
if [[ $? == 0 ]]; then
    echo "Expected failure, got success instead"
    exit 1
fi
echo ".\c"

# Test that it can load an existent PID file
$TEST load /tmp/c2hat-test.pid
if [[ $? != 0 ]]; then
    echo "Unable to load PID file"
    exit 1
fi
echo ".\c"

$TEST check /tmp/c2hat-test.pid
if [[ $? != 0 ]]; then
    echo "PID file does not exist"
    exit 1
fi
echo ".\c"

$TEST check /tmp/unknown.pid >> /dev/null
if [[ $? == 0 ]]; then
    echo "PID file should not exist"
    exit 1
fi
echo ".\c"

rm /tmp/c2hat-test.pid
echo

exit 0
