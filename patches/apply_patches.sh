#!/bin/bash

count_patches() { echo $#; }

if [ $# -lt 2 ]; then
    echo "Usage: apply_patches.sh <tag|branch> [<patch>...]"
    exit 1
fi

CUR_TAG=$1
shift
PATCHES=$@
NUM_PATCHES=$(count_patches $PATCHES)
NUM_REPO_PATCHES=$(git rev-list --count HEAD ^$CUR_TAG)

# If the checkout has more patches applied than we expect, start from scratch
if [ $NUM_PATCHES -lt $NUM_REPO_PATCHES ]; then
    git reset --hard $CUR_TAG
fi

# Check each patch in order and apply it if needed
for p in $PATCHES; do
    if git apply --reverse --check $p >/dev/null 2>&1 ; then
        echo "Skipping $p (already applied)"
    else
        git am $p || exit 1 # Try to apply the patch, fail if it can't be applied
    fi
done
