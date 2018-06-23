#!/bin/bash
#
# This script will run the umapsort and churn tests to
# verify that read/write/paging is still working as
# expected.  These tests currently only test
# a single file.  As multi-file tests increase (FITS)
# additional tests will be added.
#
churn --directio -f /mnt/xfs/regression_test_churn.dat -b 10000 -c 20000 -l 1000 -d 60 
umapsort -p 100000 -b 90000 -f /mnt/xfs/regression_test_sort.dat --directio -t 128

/bin/rm -f /mnt/xfs/regression_test_churn.dat /mnt/xfs/regression_test_sort.dat
