#!/usr/bin/env bash
#takes the argument as the mount point e.g. /tmp/user
mnt=$1
echo 'create the image'
echo 'truncate -s 10M this'
truncate -s 10M this
echo 'make'
make
echo 'format the image'
echo './mkfs.a1fs -f -i 4096 this'
./mkfs.a1fs -f -i 4096 this
echo 'mount the image'
echo './a1fs this '${mnt}
./a1fs this ${mnt}
echo
echo 'test file system operations'
echo 'create a directory'
echo 'mkdir '${mnt}'/testdir'
mkdir ${mnt}/testdir
echo
echo 'display the contents of the root directory'
echo 'ls '${mnt}
ls ${mnt}
echo
echo 'create a file'
echo 'touch '${mnt}'/testdir/testfile'
touch ${mnt}/testdir/testfile
echo 'ls -l '${mnt}'/testdir/testfile'
ls -l ${mnt}/testdir/testfile
echo
echo 'display the contents of the testdir directory'
echo 'ls '${mnt}'/testdir'
ls ${mnt}/testdir
echo
echo 'Use touch to update the timestamp on a file'
echo 'touch '${mnt}'/testdir/testfile'
touch ${mnt}/testdir/testfile
echo 'ls -l '${mnt}'/testdir/testfile'
ls -l ${mnt}/testdir/testfile
echo
echo 'test truncate to expand a file'
echo 'ls -l '${mnt}'/testdir/testfile'
ls -l ${mnt}/testdir/testfile
echo 'truncate -s 10 '${mnt}'/testdir/testfile'
truncate -s 10 ${mnt}/testdir/testfile
echo 'ls -l '${mnt}'/testdir/testfile'
ls -l ${mnt}/testdir/testfile
echo
echo 'test truncate to shrink a file'
echo 'ls -l '${mnt}'/testdir/testfile'
ls -l ${mnt}/testdir/testfile
echo 'truncate -s 5 '${mnt}'/testdir/testfile'
truncate -s 5 ${mnt}/testdir/testfile
echo 'ls -l '${mnt}'/testdir/testfile'
ls -l ${mnt}/testdir/testfile
echo
echo 'add data to the file'
echo 'echo "blablabla" >> '${mnt}'/testdir/testfile'
echo "blablabla" >> ${mnt}/testdir/testfile
echo 'display the contents of the file'
echo 'cat '${mnt}'/testdir/testfile'
cat ${mnt}/testdir/testfile
echo


echo 'unmount the image'
echo 'fusermount -u '${mnt}
fusermount -u ${mnt}
echo 'mount the image again'
echo './a1fs this '${mnt}
./a1fs this ${mnt}
echo
echo 'display the contents of the root directory'
echo 'ls '${mnt}
ls ${mnt}
echo 'display the contents of the testdir directory'
echo 'ls '${mnt}'/testdir'
ls ${mnt}/testdir
echo 'display the contents of the file'
echo 'cat '${mnt}'/testdir/testfile'
cat ${mnt}/testdir/testfile
echo
# echo 'test creating a file that fills the file system'
# RET=0
# until [ ${RET} -neq 0 ]; do
    # echo 'blablablablablablablablabla' >> ${mnt}/testdir/testfile
    # RET=$?
# done
echo
echo 'unmount the image'
echo 'fusermount -u '${mnt}
fusermount -u ${mnt}
echo 'remove created directory and file'
rm -rf ${mnt}/testdir
rm this

echo 'test a too-small disk image'
truncate -s 16K this
./mkfs.a1fs -f -i 4096 this
echo 'done'