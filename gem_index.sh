#!/bin/bash
CMD_PATH=`dirname $0`
GEM_DIR=$CMD_PATH/gem

if [ $# -ne 1 ]
then
	echo -e usage:
	echo -e $0 ref.fa
	exit 1
fi

$GEM_DIR/gem-indexer -i $1 -o $1.gem 2> $1.gem.log

if [[ $? -ne 0 ]]; then
	echo -e gem-indexer error.
	exit 1
fi
