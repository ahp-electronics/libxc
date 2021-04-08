/*
    libahp_xc library to drive the AHP XC correlators
    Copyright (C) 2020  Ilia Platone

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef _AHP_XC_H
#define _AHP_XC_H

#ifdef  __cplusplus
extern "C" {
#endif
#ifdef _WIN32
#define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT extern
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * \defgroup AHP XC correlators driving library
*
* The AHP XC correlators series permit intensity cross-correlation and auto-correlation
* counting from pulse detectors ranging from radio to photon counters to geiger-mode detectors
* or noise-regime or light scattering counters.
* This software is meant to work with the XC series cross-correlator FPGA, programmed with the
* Verilog firmware available online at https://github.com/ahp-electronics/xc-firmware
*
* \author Ilia Platone
*/
/*@{*/

///AHP_XC_VERSION This library version
#define AHP_XC_VERSION 0x010014

///AHP_XC_LIVE_AUTOCORRELATOR indicates if the correlator can do live spectrum analysis
#define AHP_XC_LIVE_AUTOCORRELATOR (1<<0)
///AHP_XC_LIVE_CROSSCORRELATOR indicates if the correlator can do live cross-correlation
#define AHP_XC_LIVE_CROSSCORRELATOR (1<<1)
///AHP_XC_HAS_LED_FLAGS indicates if the correlator has led lines available to drive
#define AHP_XC_HAS_LED_FLAGS (1<<2)
///AHP_XC_HAS_CROSSCORRELATOR indicates if the correlator can cross-correlate or can autocorrelate only
#define AHP_XC_HAS_CROSSCORRELATOR (1<<3)

/**
 * \defgroup DSP_Defines DSP API defines
*/
/*@{*/

///XC_BASE_RATE is the base baud rate of the XC cross-correlators
#define XC_BASE_RATE ((int)57600)

/*@}*/

/**
 * \defgroup Enumerations
*/
/*@{*/
/**
* \brief These are the baud rates supported
*/
typedef enum {
    R_57600 = 0,
    R_115200 = 1,
    R_230400 = 2,
    R_460800 = 3,
} baud_rate;

/**
* \brief The XC firmare commands
*/
typedef enum {
    CLEAR = 0,
    SET_INDEX = 1,
    SET_LEDS = 2,
    SET_BAUD_RATE = 3,
    SET_DELAY = 4,
    SET_FREQ_DIV = 8,
    SET_VOLTAGE = 9,
    ENABLE_TEST = 12,
    ENABLE_CAPTURE = 13
} xc_cmd;

/**
* \brief The XC firmare commands
*/
typedef enum {
    TEST_NONE = 0,
    TEST_SIGNAL = 1,
    SCAN_AUTO = 2,
    SCAN_CROSS = 4,
    TEST_ALL = 0xf,
} xc_test;

/**
* \brief Correlations structure
*/
typedef struct {
    unsigned long correlations; ///Correlations count
    unsigned long counts; ///Pulses count
    double coherence; ///Coherence ratio given by correlations/counts
} ahp_xc_correlation;

/**
* \brief Sample structure
*/
typedef struct {
    unsigned long jitter_size; ///Maximum lag in a single shot
    ahp_xc_correlation *correlations; ///Correlations array, of size jitter_size in an ahp_xc_packet
} ahp_xc_sample;

/**
* \brief Packet structure
*/
typedef struct {
    unsigned long n_lines; ///Number of lines in this correlator
    unsigned long n_baselines; ///Total number of baselines obtainable
    unsigned long tau; ///Bandwidth inverse frequency
    unsigned long bps; ///Bits capacity in each sample
    unsigned long cross_lag; ///Maximum crosscorrelation lag in a single shot
    unsigned long auto_lag; ///Maximum autocorrelation lag in a single shot
    unsigned long* counts; ///Counts in the current shot
    ahp_xc_sample* autocorrelations; ///Autocorrelations in the current shot
    ahp_xc_sample* crosscorrelations; ///Crosscorrelations in the current shot
} ahp_xc_packet;

/*@}*/

/**
 * \defgroup XC Correlators API
*/
/*@{*/
/**
 * \defgroup Communication
*/
/*@{*/

/**
* \brief Connect to a serial port
* \param port The serial port name or filename
* \return Returns 0 on success, -1 if any error was encountered
* \sa ahp_xc_disconnect
*/
DLL_EXPORT int ahp_xc_connect(const char *port);

/**
* \brief Connect to a serial port or other stream with the given file descriptor
* \param fd The file descriptor of the stream
*/
DLL_EXPORT int ahp_xc_connect_fd(int fd);

/**
* \brief Disconnect from the serial port opened with ahp_xc_connect()
* \sa ahp_xc_connect
*/
DLL_EXPORT void ahp_xc_disconnect(void);

/**
* \brief Report connection status
* \sa ahp_xc_connect
* \sa ahp_xc_connect_fd
* \sa ahp_xc_disconnect
*/
DLL_EXPORT int ahp_xc_is_connected();

/**
* \brief Obtain the current baud rate
* \return Returns the baud rate
*/
DLL_EXPORT int ahp_xc_get_baudrate(void);

/**
* \brief Obtain the current baud rate
* \param rate The new baud rate index
* \param setterm Change the termios settings of the current fd or port opened
*/
DLL_EXPORT void ahp_xc_set_baudrate(baud_rate rate);

/*@}*/
/**
 * \defgroup Features of the correlator
*/
/*@{*/

/**
* \brief Obtain the current baud rate
* \return Returns 0 on success, -1 if any error was encountered
*/
DLL_EXPORT int ahp_xc_get_properties(void);

/**
* \brief Obtain the correlator header
* \return Returns a string containing the header and device identifier
*/
DLL_EXPORT char* ahp_xc_get_header();

/**
* \brief Obtain the correlator bits per sample
* \return Returns the bits per sample value
*/
DLL_EXPORT int ahp_xc_get_bps(void);

/**
* \brief Obtain the correlator number of lines
* \return Returns the number of lines
*/
DLL_EXPORT int ahp_xc_get_nlines(void);

/**
* \brief Obtain the correlator total baselines
* \return Returns the baselines quantity
*/
DLL_EXPORT int ahp_xc_get_nbaselines(void);

/**
* \brief Obtain the correlator maximum delay value
* \return Returns the delay size
*/
DLL_EXPORT int ahp_xc_get_delaysize(void);

/**
* \brief Obtain the correlator jitter buffer size for autocorrelations
* \return Returns the jitter size
*/
DLL_EXPORT int ahp_xc_get_autocorrelator_jittersize(void);

/**
* \brief Obtain the correlator jitter buffer size for crosscorrelations
* \return Returns the jitter size
*/
DLL_EXPORT int ahp_xc_get_crosscorrelator_jittersize(void);

/**
* \brief Obtain the correlator maximum readout frequency
* \return Returns the maximum readout frequency
*/
DLL_EXPORT int ahp_xc_get_frequency(void);

/**
* \brief Obtain the correlator maximum readout frequency
* \return Returns the maximum readout frequency
*/
DLL_EXPORT int ahp_xc_get_frequency_divider(void);

/**
* \brief Obtain the serial packet transmission time in microseconds
* \return Returns the packet transmission time
*/
DLL_EXPORT unsigned int ahp_xc_get_packettime(void);

/**
* \brief Obtain the serial packet size
* \return Returns the packet size
*/
DLL_EXPORT int ahp_xc_get_packetsize(void);
/*@}*/
/**
 * \defgroup Data and streaming
*/
/*@{*/

/**
* \brief Allocate and return a packet structure
* \return Returns the packet structure
*/
DLL_EXPORT ahp_xc_packet *ahp_xc_alloc_packet();

/**
* \brief Free a previously allocated packet structure
* \param packet the packet structure to be freed
*/
DLL_EXPORT void ahp_xc_free_packet(ahp_xc_packet *packet);

/**
* \brief Allocate and return a samples array
* \param nlines The Number of samples to be allocated.
* \param len The jitter_size and correlations field size of each sample.
* \return Returns the samples array
* \sa ahp_xc_free_samples
* \sa ahp_xc_sample
* \sa ahp_xc_packet
*/
DLL_EXPORT ahp_xc_sample *ahp_xc_alloc_samples(unsigned long nlines, unsigned long len);

/**
* \brief Free a previously allocated samples array
* \param packet the samples array to be freed
* \sa ahp_xc_alloc_samples
* \sa ahp_xc_sample
* \sa ahp_xc_packet
*/
DLL_EXPORT void ahp_xc_free_samples(unsigned long nlines, ahp_xc_sample *samples);

/**
* \brief Grab a data packet
* \param packet The xc_packet structure to be filled.
* \sa ahp_xc_set_lag_auto
* \sa ahp_xc_set_lag_cross
* \sa ahp_xc_alloc_packet
* \sa ahp_xc_free_packet
* \sa ahp_xc_packet
*/
DLL_EXPORT int ahp_xc_get_packet(ahp_xc_packet *packet);

/**
* \brief Initiate an autocorrelation scan
* \param index The line index.
* \param start the starting channel for this scan.
*/
DLL_EXPORT void ahp_xc_start_autocorrelation_scan(int index, int start);

/**
* \brief End an autocorrelation scan
* \param index The line index.
*/
DLL_EXPORT void ahp_xc_end_autocorrelation_scan(int index);

/**
* \brief Scan all available delay channels and get autocorrelations of each input
* \param index the index of the input chosen for autocorrelation.
* \param autocorrelations An ahp_xc_sample array pointer that this function will allocate, will be filled with the autocorrelated values and will be of size ahp_xc_delaysize.
* \param percent A pointer to a double which, during scanning, will be updated with the percent of completion.
* \param interrupt A pointer to an integer whose value, during execution, if turns into 1 will abort scanning.
* \sa ahp_xc_get_delaysize
* \sa ahp_xc_sample
*/
DLL_EXPORT int ahp_xc_scan_autocorrelations(int index, ahp_xc_sample **autocorrelations, int start, unsigned int len, int *interrupt, double *percent);

/**
* \brief Initiate a crosscorrelation scan
* \param index The line index.
* \param start the starting channel for this scan.
*/
DLL_EXPORT void ahp_xc_start_crosscorrelation_scan(int index, int start);

/**
* \brief End a crosscorrelation scan
* \param index The line index.
*/
DLL_EXPORT void ahp_xc_end_crosscorrelation_scan(int index);

/**
* \brief Scan all available delay channels and get crosscorrelations of each input with others
* \param index1 the index of the first input in this baseline.
* \param index2 the index of the second input in this baseline.
* \param crosscorrelations An ahp_xc_sample array pointer that this function will allocate, will be filled with the crosscorrelated values and will be of size ahp_xc_delaysize*2-1.
* \param percent A pointer to a double which, during scanning, will be updated with the percent of completion.
* \param interrupt A pointer to an integer whose value, during execution, if turns into 1 will abort scanning.
* \sa ahp_xc_get_delaysize
* \sa ahp_xc_sample
*/
DLL_EXPORT int ahp_xc_scan_crosscorrelations(int index1, int index2, ahp_xc_sample **crosscorrelations, unsigned int start1, unsigned int start2, unsigned int size, int *interrupt, double *percent);

/*@}*/
/**
 * \defgroup Commands and setup of the correlator
*/
/*@{*/

/**
* \brief Enable capture by starting serial transmission from the correlator
* \param enable 1 to enable capture, 0 to stop capturing.
*/
DLL_EXPORT int ahp_xc_enable_capture(int enable);

/**
* \brief Switch on or off the led lines of the correlator
* \param index The input line index starting from 0
* \param leds The enable mask of the leds
*/
DLL_EXPORT void ahp_xc_set_leds(int index, int leds);

/**
* \brief Set the lag of the selected input in clock cycles (for cross-correlation)
* \param index The input line index starting from 0
* \param value The lag amount in clock cycles
*/
DLL_EXPORT void ahp_xc_set_lag_cross(int index, int value);

/**
* \brief Set the lag of the selected input in clock cycles (for auto-correlation)
* \param index The input line index starting from 0
* \param value The lag amount in clock cycles
*/
DLL_EXPORT void ahp_xc_set_lag_auto(int index, int value);

/**
* \brief Set the clock divider for autocorrelation and crosscorrelation
* \param value The clock divider power of 2
*/
DLL_EXPORT void ahp_xc_set_frequency_divider(unsigned char value);

/**
* \brief Set the supply voltage on the current line
* \param index The input line index starting from 0
* \param value The voltage level
*/
DLL_EXPORT void ahp_xc_set_voltage(int index, unsigned char value);

/**
* \brief Enable tests on the current line
* \param index The input line index starting from 0
* \param value The test type
*/
DLL_EXPORT void ahp_xc_set_test(int index, xc_test value);

/**
* \brief Disable tests on the current line
* \param index The input line index starting from 0
* \param value The test type
*/
DLL_EXPORT void ahp_xc_clear_test(int index, xc_test value);

/**
* \brief Send an arbitrary command to the AHP xc device
* \param cmd The command
* \param value The command parameter
*/
DLL_EXPORT ssize_t ahp_xc_send_command(xc_cmd cmd, unsigned char value);

/**
* \brief Obtain the current libahp-xc version
*/
DLL_EXPORT inline unsigned int ahp_xc_get_version() { return AHP_XC_VERSION; }

/*@}*/
/*@}*/
/*@}*/
#ifdef __cplusplus
} // extern "C"
#endif

#endif //_AHP_XC_H


