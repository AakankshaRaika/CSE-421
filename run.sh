cd $HOME/os161/kern/compile/ASST2
bmake depend

if [ $? -eq 0 ] ; then
		echo "INFO: bmake depend successful, executing bmake..."
else
		echo "$(tput setaf 009) INFO: error running bmake depend$(tput setaf 260)"
		exit
fi

bmake

if [ $? -eq 0 ] ; then
		echo "INFO: bmake successful, executing bmake install..."
else
		echo "$(tput setaf 009)INFO: error running bmake$(tput setaf 260)"
		exit
fi

bmake install

if [ $? -eq 0 ] ; then
		echo "INFO: bmake install successful, activating kernel..."
else
		echo "$(tput setaf 009)INFO: error running bmake install$(tput setaf 260)"
		exit
fi

cd $HOME/os161/root
sys161 kernel

