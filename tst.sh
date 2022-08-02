#!/bin/sh

# Remove old log files.
rm -f x._???

./lgr_test

echo "Testing perf."

# Skip "Opening", "Starting", and first log (to warm up cache).
FILE=`./lgr_perf`
START=`sed -n "4s/^.......... ..:..:..\.\(......\).*$/\1/p" $FILE`
END=`sed -n "4003s/^.......... ..:..:..\.\(......\).*$/\1/p" $FILE`
# Prepend a '1' so Perl doesn't interpret leading zero as octal.
perl -e "print '($END - $START) / 4000 = ' . (1$END - 1$START) / 4000 . \"\n\";"

FILE=`./lgr_perf`
START=`sed -n "4s/^.......... ..:..:..\.\(......\).*$/\1/p" $FILE`
END=`sed -n "4003s/^.......... ..:..:..\.\(......\).*$/\1/p" $FILE`
# Prepend a '1' so Perl doesn't interpret leading zero as octal.
perl -e "print '($END - $START) / 4000 = ' . (1$END - 1$START) / 4000 . \"\n\";"

FILE=`./lgr_perf`
START=`sed -n "4s/^.......... ..:..:..\.\(......\).*$/\1/p" $FILE`
END=`sed -n "4003s/^.......... ..:..:..\.\(......\).*$/\1/p" $FILE`
# Prepend a '1' so Perl doesn't interpret leading zero as octal.
perl -e "print '($END - $START) / 4000 = ' . (1$END - 1$START) / 4000 . \"\n\";"

FILE=`./lgr_perf`
START=`sed -n "4s/^.......... ..:..:..\.\(......\).*$/\1/p" $FILE`
END=`sed -n "4003s/^.......... ..:..:..\.\(......\).*$/\1/p" $FILE`
# Prepend a '1' so Perl doesn't interpret leading zero as octal.
perl -e "print '($END - $START) / 4000 = ' . (1$END - 1$START) / 4000 . \"\n\";"

FILE=`./lgr_perf`
START=`sed -n "4s/^.......... ..:..:..\.\(......\).*$/\1/p" $FILE`
END=`sed -n "4003s/^.......... ..:..:..\.\(......\).*$/\1/p" $FILE`
# Prepend a '1' so Perl doesn't interpret leading zero as octal.
perl -e "print '($END - $START) / 4000 = ' . (1$END - 1$START) / 4000 . \"\n\";"

FILE=`./lgr_perf`
START=`sed -n "4s/^.......... ..:..:..\.\(......\).*$/\1/p" $FILE`
END=`sed -n "4003s/^.......... ..:..:..\.\(......\).*$/\1/p" $FILE`
# Prepend a '1' so Perl doesn't interpret leading zero as octal.
perl -e "print '($END - $START) / 4000 = ' . (1$END - 1$START) / 4000 . \"\n\";"

FILE=`./lgr_perf`
START=`sed -n "4s/^.......... ..:..:..\.\(......\).*$/\1/p" $FILE`
END=`sed -n "4003s/^.......... ..:..:..\.\(......\).*$/\1/p" $FILE`
# Prepend a '1' so Perl doesn't interpret leading zero as octal.
perl -e "print '($END - $START) / 4000 = ' . (1$END - 1$START) / 4000 . \"\n\";"
