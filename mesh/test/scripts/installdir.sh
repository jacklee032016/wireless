# /bin/sh
# ����ű��ڱ���ʱ����

DIRS="/drivers /lib /bin"
	
	BINDIRNAME=`echo $1 | sed 's/\/\//\//g'`
	
	for directory in $DIRS; do
		echo make dir : $BINDIRNAME$directory;
		mkdir -p $BINDIRNAME$directory
	done
