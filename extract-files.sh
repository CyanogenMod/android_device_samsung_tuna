#!/bin/bash

if [ $# -eq 1 ]; then
    COPY_FROM=$1
    test ! -d "$COPY_FROM" && echo error reading dir "$COPY_FROM" && exit 1
fi

test -z "$DEVICE" && echo device not set && exit 2
test -z "$VENDOR" && echo vendor not set && exit 2
test -z "$VENDORDEVICEDIR" && VENDORDEVICEDIR=$DEVICE
export VENDORDEVICEDIR

BASE=../../../vendor/$VENDOR/$VENDORDEVICEDIR/proprietary
rm -rf $BASE/*

for FILE in `egrep -v '(^#|^$)' ../$DEVICE/device-proprietary-files.txt`; do
    echo "Extracting /system/$FILE ..."
    OLDIFS=$IFS IFS=":" PARSING_ARRAY=($FILE) IFS=$OLDIFS
    FILE=${PARSING_ARRAY[0]}
    DEST=${PARSING_ARRAY[1]}
    if [ -z $DEST ]
    then
        DEST=$FILE
    fi
    DIR=`dirname $FILE`
    if [ ! -d $BASE/$DIR ]; then
        mkdir -p $BASE/$DIR
    fi
    if [ "$COPY_FROM" = "" ]; then
        adb pull /system/$FILE $BASE/$DEST
        # if file dot not exist try destination
        if [ "$?" != "0" ]
          then
          adb pull /system/$DEST $BASE/$DEST
        fi
    else
        cp $COPY_FROM/$FILE $BASE/$DEST
        # if file does not exist try destination
        if [ "$?" != "0" ]
            then
            cp $COPY_FROM/$DEST $BASE/$DEST
        fi
    fi
done

for FILE in `egrep -v '(^#|^$)' ../tuna/proprietary-files.txt`; do
    echo "Extracting /system/$FILE ..."
    OLDIFS=$IFS IFS=":" PARSING_ARRAY=($FILE) IFS=$OLDIFS
    FILE=${PARSING_ARRAY[0]}
    DEST=${PARSING_ARRAY[1]}
    if [ -z $DEST ]
    then
        DEST=$FILE
    fi
    DIR=`dirname $FILE`
    if [ ! -d $BASE/$DIR ]; then
        mkdir -p $BASE/$DIR
    fi
    if [ "$COPY_FROM" = "" ]; then
        adb pull /system/$FILE $BASE/$DEST
        # if file dot not exist try destination
        if [ "$?" != "0" ]
          then
          adb pull /system/$DEST $BASE/$DEST
        fi
    else
        cp $COPY_FROM/$FILE $BASE/$DEST
        # if file does not exist try destination
        if [ "$?" != "0" ]
            then
            cp $COPY_FROM/$DEST $BASE/$DEST
        fi
    fi
done

BASE=../../../vendor/$VENDOR/tuna/proprietary
rm -rf $BASE/*
for FILE in `egrep -v '(^#|^$)' ../tuna/common-proprietary-files.txt`; do
    echo "Extracting /system/$FILE ..."
    OLDIFS=$IFS IFS=":" PARSING_ARRAY=($FILE) IFS=$OLDIFS
    FILE=${PARSING_ARRAY[0]}
    DEST=${PARSING_ARRAY[1]}
    if [ -z $DEST ]
    then
        DEST=$FILE
    fi
    DIR=`dirname $FILE`
    if [ ! -d $BASE/$DIR ]; then
        mkdir -p $BASE/$DIR
    fi
    if [ "$COPY_FROM" = "" ]; then
        adb pull /system/$FILE $BASE/$DEST
        # if file dot not exist try destination
        if [ "$?" != "0" ]
          then
          adb pull /system/$DEST $BASE/$DEST
        fi
    else
        cp $COPY_FROM/$FILE $BASE/$DEST
        # if file does not exist try destination
        if [ "$?" != "0" ]
            then
            cp $COPY_FROM/$DEST $BASE/$DEST
        fi
    fi
done

echo "This is designed to extract files from an official cm-11 build"
../tuna/setup-makefiles.sh
