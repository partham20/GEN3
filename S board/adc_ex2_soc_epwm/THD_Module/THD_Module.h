//#############################################################################
//! \file THD_Module.h
//! \brief Single-include header for the THD Analyzer Module
//!
//! Drop the THD_Module/ folder into any C2000 project, add THD_Module.c
//! to the build, add THD_Module/ to include paths, and use:
//!
//!   #include "THD_Module.h"
//!
//! INTEGRATION EXAMPLE:
//!   THD_Analyzer myAnalyzer;
//!   THD_init(&myAnalyzer, inBuf, outBuf, magBuf, phaseBuf,
//!            twiddleBuf, windowBuf, chData, 5, 512, 9);
//!   THD_loadChannelData(&myAnalyzer, 0, mySamples, 512);
//!   THD_processChannel(&myAnalyzer, 0);
//!   float thd = THD_getResult(&myAnalyzer, 0)->thdPercent;
//#############################################################################

#ifndef THD_MODULE_H
#define THD_MODULE_H

#include "thd_config.h"
#include "thd_signal_processing.h"
#include "thd_analyzer.h"
#include "thd_adc_driver.h"

#endif // THD_MODULE_H
