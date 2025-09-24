#!/bin/bash

# Run `go test -v` in all subdirectories of the current directory
set -e

for dir in */ ; do
	if [ "$dir" = "e2e/" ]; then
		# ./e2e/test.sh
		continue
	fi
	if [ -d "$dir" ]; then
		if [ -f "$dir/go.mod" ] || [ -n "$(find "$dir" -maxdepth 1 -name '*.go' -print -quit)" ]; then
			echo -e "\n===== Running tests in $dir ====="
			(cd "$dir" && go test -v)
		fi
	fi
done