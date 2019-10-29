#!/bin/bash

# Usage example:
# ./backup.sh -c catalog_name -a archive_name -e "ext1 ext2 cpp hpp py java"
# Decrypt archive:
# openssl enc -d -aes-256-cbc -md sha512 -pbkdf2 -iter 1000 -in "archive.tar.gz" | tar xz


#################################
# Processing options/parameters #
#################################

backup_path=/home/vova/Desktop/Android
# backup_path=$HOME
catalog=backup_$(date +%Y-%m-%d_%H%M%S)
archive=backup_$(date +%Y-%m-%d_%H%M%S)
extensions=("h hpp cpp c")

unexpected () {
    echo "Unexpected parameter/option: \"$1\". Terminating." 1>&2
    exit -1
}

assign_archive () {
    if [ -z "$1" ]; then
        echo "Archive name expected. Terminating." 1>&2
        exit -2
    fi
    archive=$1
}

assign_catalog () {
    if [ -z "$1" ]; then
        echo "Catalog name expected. Terminating." 1>&2
        exit -3
    fi
    catalog=$1
}

assign_extenstions () {
    if [ -z "$1" ]; then
        echo "Extensions list expected. Terminating." 1>&2
        exit -4
    fi
    extensions=("$1")
}

while [ -n "$1" ]; do
    case "$1" in
        -a) assign_archive "$2"
            shift;;
        -c) assign_catalog "$2"
            shift;;
        -e) assign_extenstions "$2"
            shift;;
        *) unexpected;;
    esac
    shift
done

if [ -d "$catalog" ]; then
    echo "Specified catalog name already exists. Terminating." 1>&2
    exit -5
fi


######################
# Copying to catalog #
######################

is_valid_extension () {
    local is=1
    for e in $extensions; do
        if [[ "$e" == "$1" ]]; then
            is=0
            break
        fi
    done
    return $is
}

mkdir "$catalog"
find $backup_path -type f | while read f; do
    filename=$(basename -- "$f")
    extension="${filename##*.}"
    if is_valid_extension $extension; then
        rf=${f#*${backup_path}}
        rd=$(dirname -- "$rf")
        mkdir -p "${catalog}/${rd}"
        cp "$f" "${catalog}$rf"
    fi
done


###################################
# Archive creation and encryption #
###################################

tar -czf - "${catalog}" \
| openssl enc -e -aes-256-cbc -md sha512 -pbkdf2 -iter 1000 -out "/tmp/${archive}.tar.gz" -pass pass:12345 \
&& rm -R "${catalog}" \
&& echo "done"
