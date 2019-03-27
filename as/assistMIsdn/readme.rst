Framework of ISDN driver
##########################################
Li Zhijie, 2019,03.27(2005.07.30)


Built modules
========================
#. mcore: isdn.ko, core of ISDN drivers for different chipsets and protocol stack;
#. device/assist: hfc.ko, driver for HFC ISDN chipsets; ISR and bottom half;
#. protocols/layer1: kl1.ko, L1;
#. protocols/layer2: kl2.ko, L2 and TEI (Terminal Endpoint Identifier);
#. protocols/uins: ins.ko, L3 protocol
	
				