const int LED_PIN = 5;
const int RF_DATA_PIN = 2;
const int RF_CLK_PIN = 14;
const int RF_DATA_EXTIN = RF_DATA_PIN;
const int SAFE_BOOT_PIN = 8;
const int MAIN_CLOCK_MHZ = 12;

class App {
public:
  class : public ookey::rx::Decoder {
  public:
    void dataReceived(unsigned short address, unsigned char *data, int len) {
      // target::PORT.OUTTGL.setOUTTGL(1 << LED_PIN);
    }

  } decoder;

  class : public usb::Device {

  } usb;

  void init() {
    atsamd::safeboot::init(SAFE_BOOT_PIN, false, LED_PIN);

    target::gclk::GENDIV::Register workGendiv;
    target::gclk::GENCTRL::Register workGenctrl;
    target::gclk::CLKCTRL::Register workClkctrl;

    //////////////////////////////////////////////////////////////////////////
    // XOSC: enable external 12MHz crystal
    //////////////////////////////////////////////////////////////////////////

    target::SYSCTRL.XOSC.setGAIN(target::sysctrl::XOSC::GAIN::_16MHZ);
    target::SYSCTRL.XOSC.setXTALEN(true);
    target::SYSCTRL.XOSC.setENABLE(true);

    //////////////////////////////////////////////////////////////////////////
    // GEN0: output 12MHz
    //////////////////////////////////////////////////////////////////////////

    target::GCLK.GENCTRL = workGenctrl.zero()
                               .setID(0)
                               .setSRC(target::gclk::GENCTRL::SRC::XOSC)
                               .setGENEN(true);
    while (target::GCLK.STATUS.getSYNCBUSY())
      ;

    //////////////////////////////////////////////////////////////////////////
    // GEN1: output 32kHz
    //////////////////////////////////////////////////////////////////////////

    target::GCLK.GENDIV = workGendiv.zero().setID(1).setDIV(375);
    while (target::GCLK.STATUS.getSYNCBUSY())
      ;

    target::GCLK.GENCTRL = workGenctrl.zero()
                               .setID(1)
                               .setSRC(target::gclk::GENCTRL::SRC::XOSC)
                               .setGENEN(true);
    while (target::GCLK.STATUS.getSYNCBUSY())
      ;

    //////////////////////////////////////////////////////////////////////////
    // PLL96: output 54.2766MHz
    //////////////////////////////////////////////////////////////////////////

    target::GCLK.CLKCTRL = workClkctrl.zero()
                               .setID(target::gclk::CLKCTRL::ID::FDPLL)
                               .setGEN(target::gclk::CLKCTRL::GEN::GCLK1)
                               .setCLKEN(true);
    while (target::GCLK.STATUS.getSYNCBUSY())
      ;

    target::SYSCTRL.DPLLCTRLB.setREFCLK(
        target::sysctrl::DPLLCTRLB::REFCLK::GCLK);

    target::SYSCTRL.DPLLRATIO.setLDR(1695);
    target::SYSCTRL.DPLLRATIO.setLDRFRAC(2);

    target::SYSCTRL.DPLLCTRLA.setENABLE(true);

    //////////////////////////////////////////////////////////////////////////
    // GEN4: output 27.1383MHz to external RF_CLK_PIN
    //////////////////////////////////////////////////////////////////////////

    target::GCLK.GENDIV = workGendiv.zero().setID(4).setDIV(2);
    while (target::GCLK.STATUS.getSYNCBUSY())
      ;

    target::GCLK.GENCTRL = workGenctrl.zero()
                               .setID(4)
                               .setSRC(target::gclk::GENCTRL::SRC::DPLL96M)
                               .setOE(true)
                               .setGENEN(true);
    while (target::GCLK.STATUS.getSYNCBUSY())
      ;

    target::PORT.PMUX[RF_CLK_PIN >> 1].setPMUXE(target::port::PMUX::PMUXE::H);
    target::PORT.PINCFG[RF_CLK_PIN].setPMUXEN(true);

    //////////////////////////////////////////////////////////////////////////
    // FLL48: output 48MHz
    //////////////////////////////////////////////////////////////////////////

    // See Errata 9905
    {
      target::sysctrl::DFLLCTRL::Register r;
      target::SYSCTRL.DFLLCTRL = r.zero().setENABLE(true).setMODE(true);
      while (!target::SYSCTRL.PCLKSR.getDFLLRDY())
        ;
    }

    target::GCLK.CLKCTRL = workClkctrl.zero()
                               .setID(target::gclk::CLKCTRL::ID::DFLL48)
                               .setGEN(target::gclk::CLKCTRL::GEN::GCLK1)
                               .setCLKEN(true);
    while (target::GCLK.STATUS.getSYNCBUSY())
      ;

    {
      target::sysctrl::DFLLCTRL::Register r;
      target::SYSCTRL.DFLLCTRL = r.zero().setENABLE(true).setMODE(true);
      while (!target::SYSCTRL.PCLKSR.getDFLLRDY())
        ;
    }

    {
      target::sysctrl::DFLLMUL::Register r;
      target::SYSCTRL.DFLLMUL = r.zero().setMUL(48e6 / 32e3);
      while (!target::SYSCTRL.PCLKSR.getDFLLRDY())
        ;
    }

    while (!target::SYSCTRL.PCLKSR.getDFLLRDY() ||
           !target::SYSCTRL.PCLKSR.getDFLLLCKC() ||
           !target::SYSCTRL.PCLKSR.getDFLLLCKF())
      ;

    //////////////////////////////////////////////////////////////////////////
    // GEN5: output 48MHz
    //////////////////////////////////////////////////////////////////////////

    target::GCLK.GENCTRL = workGenctrl.zero()
                               .setID(5)
                               .setSRC(target::gclk::GENCTRL::SRC::DFLL48M)
                               .setGENEN(true);

    //                            .setOE(true);
    // target::PORT.PMUX[7].setPMUXO(target::port::PMUX::PMUXO::H);
    // target::PORT.PINCFG[15].setPMUXEN(true);

    while (target::GCLK.STATUS.getSYNCBUSY())
      ;

    //////////////////////////////////////////////////////////////////////////
    // GPIO
    //////////////////////////////////////////////////////////////////////////

    target::PM.APBBMASK.setPORT(1);
    target::PORT.DIRSET.setDIRSET(1 << LED_PIN);
    target::PORT.DIRCLR.setDIRCLR(1 << RF_DATA_PIN);
    target::PORT.PINCFG[RF_DATA_PIN].setINEN(true);
    target::PORT.PINCFG[RF_DATA_PIN].setPMUXEN(true);

    //////////////////////////////////////////////////////////////////////////
    // EIC
    //////////////////////////////////////////////////////////////////////////

    target::GCLK.CLKCTRL = workClkctrl.zero()
                               .setID(target::gclk::CLKCTRL::ID::EIC)
                               .setGEN(target::gclk::CLKCTRL::GEN::GCLK0)
                               .setCLKEN(true);
    while (target::GCLK.STATUS.getSYNCBUSY())
      ;

    target::EIC.CONFIG.setSENSE(RF_DATA_EXTIN,
                                target::eic::CONFIG::SENSE::RISE);

    target::EIC.CONFIG.setFILTEN(RF_DATA_EXTIN, true);

    target::EIC.CTRL.setENABLE(true);

    target::NVIC.ISER.setSETENA(1 << target::interrupts::External::EIC);

    //////////////////////////////////////////////////////////////////////////
    // TC1: generate interrupts 1ms and 2.5ms after rising edge on RF pin
    //////////////////////////////////////////////////////////////////////////

    target::GCLK.CLKCTRL = workClkctrl.zero()
                               .setID(target::gclk::CLKCTRL::ID::TC1_TC2)
                               .setGEN(target::gclk::CLKCTRL::GEN::GCLK0)
                               .setCLKEN(true);
    while (target::GCLK.STATUS.getSYNCBUSY())
      ;

    target::PM.APBCMASK.setTC(1, true);

    target::TC1.COUNT16.CTRLA.setMODE(
        target::tc::COUNT16::CTRLA::MODE::COUNT16);

    // TC1: tick at 12/8us
    target::TC1.COUNT16.CTRLA.setPRESCALER(
        target::tc::COUNT16::CTRLA::PRESCALER::DIV8);

    target::TC1.COUNT16.CTRLBSET.setONESHOT(true);
    target::TC1.COUNT16.CTRLBSET.setCMD(
        target::tc::COUNT16::CTRLBSET::CMD ::STOP);
    target::TC1.COUNT16.CC[0] = 1000 * MAIN_CLOCK_MHZ / 8;
    target::TC1.COUNT16.INTENSET.setMC(0, true);
    target::TC1.COUNT16.CC[1] = 2500 * MAIN_CLOCK_MHZ / 8;
    target::TC1.COUNT16.INTENSET.setMC(1, true);

    target::TC1.COUNT16.CTRLA.setENABLE(true);

    target::NVIC.ISER.setSETENA(1 << target::interrupts::External::TC1);

    //////////////////////////////////////////////////////////////////////////
    // initialize USB
    //////////////////////////////////////////////////////////////////////////

    target::GCLK.CLKCTRL = workClkctrl.zero()
                               .setID(target::gclk::CLKCTRL::ID::USB)
                               .setGEN(target::gclk::CLKCTRL::GEN::GCLK5);
    while (target::GCLK.STATUS.getSYNCBUSY())
      ;

    target::NVIC.ISER.setSETENA(1 << target::interrupts::External::USB);
    usb.init();

    //////////////////////////////////////////////////////////////////////////
    // initialize decoder and enable interrupt on RF data pin
    //////////////////////////////////////////////////////////////////////////

    decoder.init(1);
    // decoder.promisc = true;

    target::EIC.INTENSET.setEXTINT(RF_DATA_EXTIN, true);
  }

} app;

void interruptHandlerTC1() {
  if (target::TC1.COUNT16.INTFLAG.getMC(0)) {
    target::TC1.COUNT16.INTFLAG.setMC(0, true);
    app.decoder.pushBit((target::PORT.IN.getIN() >> RF_DATA_PIN) & 1);
  }

  if (target::TC1.COUNT16.INTFLAG.getMC(1)) {
    target::TC1.COUNT16.INTFLAG.setMC(1, true);
    app.decoder.restart();
  }
}

void interruptHandlerEIC() {
  if (target::EIC.INTFLAG.getEXTINT(RF_DATA_EXTIN)) {
    target::TC1.COUNT16.CTRLBSET.setCMD(
        target::tc::COUNT16::CTRLBSET::CMD ::STOP);
    target::TC1.COUNT16.CTRLBSET.setCMD(
        target::tc::COUNT16::CTRLBSET::CMD ::RETRIGGER);
    target::EIC.INTFLAG.setEXTINT(RF_DATA_EXTIN, true);
  }
}

void interruptHandlerUSB() {
  target::PORT.OUTSET = 1 << LED_PIN;
  app.usb.handleInterrupt();
}

void initApplication() {
  genericTimer::clkHz = MAIN_CLOCK_MHZ * 1E6;
  app.init();
}
