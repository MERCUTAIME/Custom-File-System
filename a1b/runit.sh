#!/usr/bin/env bash
echo 'create the image'
echo 'truncate -s 10M this'
truncate -s 10M this
echo 'format the image'
echo './mkfs.a1fs -i -f 4096 this'
./mkfs.a1fs -i -f 4096 this
echo 'mount the image'
echo './a1fs this /tmp/mercutaime'
./a1fs this /tmp/mercutaime

echo 'test file system operations'
echo 'create a directory'
echo 'mkdir /tmp/mercutaime/testdir'
mkdir /tmp/mercutaime/testdir
echo 'display the contents of the root directory'
echo 'ls /tmp/mercutaime'
ls /tmp/mercutaime
echo 'create a file'
echo 'touch /tmp/mercutaime/testdir/testfile'
touch /tmp/mercutaime/testdir/testfile
echo 'display the contents of the testdir directory'
echo 'ls /tmp/mercutaime/testdir'
ls /tmp/mercutaime/testdir
echo 'add data to the file'
echo 'echo "blablabla" >> /tmp/mercutaime/testdir/testfile'
echo "blablabla" >> /tmp/mercutaime/testdir/testfile
echo 'display the contents of the file'
echo 'cat /tmp/mercutaime/testdir/testfile'
cat /tmp/mercutaime/testdir/testfile

echo 'unmount the image'
echo 'fusermount -u /tmp/mercutaime'
fusermount -u /tmp/mercutaime
echo 'mount the image again'
echo './a1fs this /tmp/mercutaime'
./a1fs this /tmp/mercutaime

echo 'display the contents of the root directory'
echo 'ls /tmp/mercutaime'
ls /tmp/mercutaime
echo 'display the contents of the testdir directory'
echo 'ls /tmp/mercutaime/testdir'
ls /tmp/mercutaime/testdir
echo 'display the contents of the file'
echo 'cat /tmp/mercutaime/testdir/testfile'
cat /tmp/mercutaime/testdir/testfile

echo 'unmount the image'
echo 'fusermount -u /tmp/mercutaime'
fusermount -u /tmp/mercutaime