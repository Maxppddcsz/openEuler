#!/usr/bin/env bash
#set -x

PATCH_DIR=/local/work/SGX/SGX-host-virt-Backport-UniCloud-20220317
PATCH_DIR_MISS="${PATCH_DIR}.miss"
REPO_DIR="$(pwd)"

function copy_missing_patches()
{
	local missing_commits=$1

	rm -rf ${PATCH_DIR_MISS}
	mkdir -p ${PATCH_DIR_MISS}

	OLDIFS=$IFS
	IFS=$'\n'
	for i in $(echo -e "$missing_commits"); do
		local p=$(echo $i | cut -d':' -f1)
		cp ${PATCH_DIR}/${p} ${PATCH_DIR_MISS}
	done
	IFS=$OLDIFS
}

function main()
{
	local line=""
	local missing_commits=""
	local commit_list="$(grep -o "^commit .\+ upstream" ${PATCH_DIR}/*)"
	local total_commit_nr=0
	local find_commit_nr=0

	cd $REPO_DIR
	echo "Find commits:"
	echo "----------------------------------------+------------+--------------------"
	echo "Old Commit                              | New Commit | Subject"
	echo "----------------------------------------|------------|--------------------"
	OLDIFS=$IFS
	IFS=$'\n'
	for i in $commit_list; do
		local c=$(echo $i | cut -f2 -d" ")
		(( ++total_commit_nr ))
		if git merge-base --is-ancestor $c HEAD 2>/dev/null ; then
			# Current branch contains the commit directly
			line="$(git log $c -1 --oneline)"
		else
			# The commit might have been backported to current branch
			line="$(git log --grep=$c --oneline)"
		fi
		if [ -n "$line" ]; then
			(( ++find_commit_nr ))
			# Replace the first space to "|"
			line=${line/ /|}
			echo "$c|$line"
		else
			missing_commits="${missing_commits}\n$(basename ${i})"
		fi
	done
	IFS=$OLDIFS

	local missing_commit_nr=$(( $total_commit_nr - $find_commit_nr ))
	echo -e "\nMissing commits (${missing_commit_nr}/${total_commit_nr}):"
	echo -n "-------------------------------"
	echo -e "$missing_commits"

	read -p "Do you want to copy the missing patches to [${PATCH_DIR_MISS}]? [y/N]" answer
	if [ X"$answer" == X"y" ]; then
		copy_missing_patches "$missing_commits"
	else
		echo "Do nothing"
	fi
}

main "$*"
