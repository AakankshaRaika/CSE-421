cd $HOME/CSE-421/kern/compile/ASST2
bmake depend

if [ $? -eq 0 ] ; then
		echo "INFO: bmake depend successful, executing bmake..."
else
		echo "INFO: error running bmake depend"
		exit
fi

bmake

if [ $? -eq 0 ] ; then
		echo "INFO: bmake successful, executing bmake install..."
else
		echo "INFO: error running bmake"
		exit
fi

bmake install

if [ $? -eq 0 ] ; then
		echo "INFO: bmake install successful, activating kernel..."
else
		echo "INFO: error running bmake install"
		exit
fi

cd $HOME/CSE-421/root
sys161 kernel

