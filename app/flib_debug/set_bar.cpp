#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <stdint.h>
#include <unistd.h>

#include <flib.h>

int main()
{
	rorcfs_device *dev = NULL;	
	rorcfs_bar *bar = NULL;
        rorcfs_dma_channel *ch = NULL;
          
        int channel = 0;
          
	dev = new rorcfs_device();
	if ( dev->init(0) == -1 ) {
		printf("failed to initialize device 0\n");
		return -1;
	}

	// bind to BAR1
	bar = new rorcfs_bar(dev, 1);
	if ( bar->init() == -1 ) {
		printf("BAR1 init failed\n");
		return -1;
	}

        ch = new rorcfs_dma_channel();
        // bind channel to BAR1, channel offset 0
        ch->init(bar, (channel+1)*RORC_CHANNEL_OFFSET);

	//struct misc_bits { 
	//  unsigned int led0 : 1;
        //  unsigned int      : 1;
	//  unsigned int led1 : 1;
	//  unsigned int test : 3;
	//};
        // 
	//union misc_reg {uint32_t reg; struct misc_bits bits;};
        // 
        //union misc_reg reg;

        //ch->setPKT(RORC_REG_MISC_CFG, 0);
        //reg.reg = ch->getPKT(RORC_REG_MISC_CFG);
        //printf("pkt misc cfg: %08x\n", reg.reg);
        //reg.bits.led0 = 1;
        //ch->setPKT(RORC_REG_MISC_CFG, reg.reg);
        //reg.reg = ch->getPKT(RORC_REG_MISC_CFG);
        //printf("pkt misc cfg: %08x\n", reg.reg);
        // 
        ////printf("misc sts: %08x\n", ch->getPKT(RORC_REG_MISC_STS));
        ////printf("misc gtx sts: %08x\n", ch->getGTX(RORC_REG_GTX_MISC_STS));
        // 
        //uint32_t mem = ch->getGTX(RORC_MEM_BASE_CTRL);
        //printf("MEM: %08x\n", mem);
        // 
        ////        ch->setGTX(RORC_MEM_BASE_CTRL, 0xFF);


	if (bar)
		delete bar;
	if (dev)
		delete (dev);

	return 0;
}
