// This is a configuration header file. Edit it depending if you want the hardcoded startup behavior.
#ifndef HARD_CODED_STARTUP_H
#define HARD_CODED_STARTUP_H

// 0 means no HardCodedStartup, 1 means HardCodedStartup is enabled
// The satellite should be built for testing with HardCodedStartup disabled, so that the radio won't turn on
// accidentally during testing For the flight build, HardCodedStartup should be enabled so that the radio will turn on
// automatically after booting
#define DEFAULT_STARTUP_VALUE 0

#endif  // HARD_CODED_STARTUP_H
