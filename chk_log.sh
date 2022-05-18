#!/bin/sh
# chk_log.sh - compare the last line of a log file to a string.
# Return 0 (success) if equal, 1 (error) if not.

# Option processing

FILE=
STRING=

while getopts "f:s:" OPTION
do
  case $OPTION in
    f) FILE="$OPTARG" ;;
    s) STRING="$OPTARG" ;;
    \?) usage ;;
  esac
done
shift `expr $OPTIND - 1`  # Make $1 the first positional param after options

if [ "$FILE" = "" ]; then echo "Missing -f" >&2; exit 1; fi
if [ "$STRING" = "" ]; then echo "Missing -s" >&2; exit 1; fi
if [ "$1" != "" ]; then echo "Unrecognized parameter '$1'" >&2; exit 1; fi

LAST="`tail -1 $FILE`"
if [ "$LAST" = "$STRING" ]; then :
  exit 0;
fi

exit 1
