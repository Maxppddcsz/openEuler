#! /bin/bash

while getopts ":a:c:i:o:" opt
do
	case $opt in
		a)
			#echo "this is -a option. OPTARGG=[$OPTARG] OPTIND=[$OPTIND]"
			aa="$OPTARG"
			;;
		c)
			#echo "this is -c option. PTARG=[$OPTARG] OPTIND=[$OPTIND]"
			cc="$OPTARG"
			;;
		i)
			if [ -z "$OPTARG" ];then
				export CONFIG=""
			else
				export CONFIG="$OPTARG"
			fi;;
		o)
			export LOG="$OPTARG"
			;;
		?)
			exit 1
			;;
	esac
done
make mrproper
make ARCH=$aa CROSS_COMPILE=$cc openeuler_defconfig
make ARCH=$aa CROSS_COMPILE=$cc menuconfig
checkrpm(){
	local package=""
	for i in $@
       	do
		if ! rpm -qa|grep -q $i &>/dev/null;then
			package+=$i
		fi
	done
	if [ -n "${package}" ];then
		yum install $package -y
	else
		echo "${package} installed"
	fi
}

checkrpm "flex bison bc ncurses-devel elfutils-devel openssl-devel"
function Make_with_Stdio(){
make ARCH=$(([ -n "${ARCH}"] && echo ${ARCH}) || echo "arm") CROSS_COMPILE=$(([ -n "${CROSS_COMPILE}"] && ${CROSS_COMPILE}) || echo "cc") -j8 > $(([ -n "${aa}" ] && echo ${aa}) || echo "make.log") | tail -f
}
cat .config > .config.old
if ! [ -f "${CONFIG}" ]; then
	echo "CONFIG Not Found."
	exit 6
else 
	cat $CONFIG > .config
fi
Make_with_Stdio $LOG
#make ARCH=$aa CROSS_COMPILE=$cc -j8
