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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include "rs232.h"
#include "ahp_xc.h"
static unsigned char *ahp_xc_test = NULL;
static unsigned char *ahp_xc_leds = NULL;
static unsigned int ahp_xc_bps = 0;
static unsigned int ahp_xc_nlines = 0;
static unsigned int ahp_xc_nbaselines = 0;
static unsigned int ahp_xc_auto_lagsize = 0;
static unsigned int ahp_xc_cross_lagsize = 0;
static unsigned int ahp_xc_delaysize = 0;
static unsigned int ahp_xc_flags = 0;
static unsigned int ahp_xc_frequency = 1;
static unsigned int ahp_xc_frequency_divider = 0;
static unsigned int ahp_xc_voltage = 0;
static unsigned int ahp_xc_connected = 0;
static size_t ahp_xc_packetsize = 4096;
static baud_rate ahp_xc_rate = R_57600;
static char ahp_xc_comport[128];
static char ahp_xc_header[16] = { 0 };
static unsigned char ahp_xc_capture_flags = 0;

static int grab_next_packet(char* buf)
{
    int err = 0;
    size_t size = ahp_xc_get_packetsize();
    memset(buf, 0, (unsigned int)size);
    if(size == 16)
        RS232_AlignFrame('\r');
    ssize_t nread = RS232_PollComport(buf, size);
    if(nread < 0) {
        err = -ETIMEDOUT;
    } else {
        off_t len = (off_t)(strchr(buf, '\r')-buf);
        if(len < size-1) {
            if(len>=16 && strncmp(ahp_xc_get_header(), buf, 16)) {
                err = -EINVAL;
            } else {
                err = -EPIPE;
            }
            RS232_AlignFrame('\r');
        }
    }
    if(strlen(buf) < size) {
        err = -ENODATA;
    }
    return err;
}

static char* grab_next_valid_packet()
{
    char *buf = (char*)malloc(ahp_xc_get_packetsize());
    int err = 0;
    int max_errored = 8;
    while (err != -ETIMEDOUT && max_errored-- > 0) {
        err = grab_next_packet(buf);
        if(!err)
            break;
    }
    if(err || max_errored < 0)
        return NULL;
    return buf;
}

static char* grab_last_packet()
{
    RS232_flushRX();
    return grab_next_valid_packet();
}

static void ahp_xc_select_input(unsigned int index)
{
    int idx = 0;
    ahp_xc_send_command(SET_INDEX, (unsigned char)((idx++)<<2)|(index&0x3));
    index >>= 2;
    ahp_xc_send_command(SET_INDEX, (unsigned char)((idx++)<<2)|(index&0x3));
    index >>= 2;
    ahp_xc_send_command(SET_INDEX, (unsigned char)((idx++)<<2)|(index&0x3));
    index >>= 2;
    ahp_xc_send_command(SET_INDEX, (unsigned char)((idx++)<<2)|(index&0x3));
}

int ahp_xc_has_crosscorrelator()
{
    return (ahp_xc_flags & AHP_XC_HAS_CROSSCORRELATOR ? 1 : 0);
}

int ahp_xc_has_psu()
{
    return (ahp_xc_flags & AHP_XC_HAS_PSU ? 1 : 0);
}

int ahp_xc_has_led_flags()
{
    return (ahp_xc_flags & AHP_XC_HAS_LED_FLAGS ? 1 : 0);
}

char* ahp_xc_get_header()
{
    return ahp_xc_header;
}

int ahp_xc_get_baudrate()
{
    return XC_BASE_RATE << ahp_xc_rate;
}

unsigned int ahp_xc_get_bps()
{
    return ahp_xc_bps;
}

unsigned int ahp_xc_get_nlines()
{
    return ahp_xc_nlines;
}

unsigned int ahp_xc_get_nbaselines()
{
    return ahp_xc_nbaselines;
}

size_t ahp_xc_get_delaysize()
{
    return ahp_xc_delaysize;
}

size_t ahp_xc_get_autocorrelator_lagsize()
{
    return ahp_xc_auto_lagsize;
}

size_t ahp_xc_get_crosscorrelator_lagsize()
{
    return ahp_xc_cross_lagsize;
}

unsigned int ahp_xc_get_frequency()
{
    return ahp_xc_frequency;
}

unsigned int ahp_xc_get_frequency_divider()
{
    return  ahp_xc_frequency_divider;
}

unsigned int ahp_xc_get_packettime()
{
    return (unsigned int)10000000 * (unsigned int)ahp_xc_get_packetsize() / (unsigned int)ahp_xc_get_baudrate();
}

size_t ahp_xc_get_packetsize()
{
    return ahp_xc_packetsize;
}

int ahp_xc_connect_fd(int fd)
{
    ahp_xc_bps = 0;
    ahp_xc_nlines = 0;
    ahp_xc_nbaselines = 0;
    ahp_xc_delaysize = 0;
    ahp_xc_frequency = 0;
    ahp_xc_packetsize = 16;
    ahp_xc_rate = R_57600;
    if(fd > -1) {
        ahp_xc_connected = 1;
        RS232_SetFD(fd);
        return -1;
    }
    return 0;
}

int ahp_xc_connect(const char *port)
{
    if(ahp_xc_connected)
        return 1;
    ahp_xc_header[0] = 0;
    int ret = 1;
    ahp_xc_bps = 0;
    ahp_xc_nlines = 0;
    ahp_xc_nbaselines = 0;
    ahp_xc_delaysize = 0;
    ahp_xc_frequency = 0;
    ahp_xc_packetsize = 16;
    ahp_xc_rate = R_57600;
    strncpy(ahp_xc_comport, port, strlen(port));
    if(!RS232_OpenComport(ahp_xc_comport))
        ret = RS232_SetupPort(XC_BASE_RATE, "8N2", 0);
    if(!ret) {
        ahp_xc_connected = 1;
    }
    return ret;
}
void ahp_xc_disconnect()
{
    if(ahp_xc_connected) {
        ahp_xc_connected = 0;
        ahp_xc_set_baudrate(R_57600);
        RS232_CloseComport();
    }
}

unsigned int ahp_xc_is_connected()
{
    return ahp_xc_connected;
}

ahp_xc_sample *ahp_xc_alloc_samples(unsigned long nlines, unsigned long size)
{
    unsigned long x;
    ahp_xc_sample* samples = (ahp_xc_sample*)malloc(sizeof(ahp_xc_sample)*nlines);
    memset(samples, 0, sizeof(ahp_xc_sample)*nlines);
    for(x = 0; x < nlines; x++) {
        samples[x].lag_size = size;
        samples[x].correlations = (ahp_xc_correlation*)malloc(sizeof(ahp_xc_correlation)*size);
        memset(samples[x].correlations, 0, sizeof(ahp_xc_correlation)*size);
    }
    return samples;
}

void ahp_xc_free_samples(unsigned long nlines, ahp_xc_sample *samples)
{
    unsigned long x;
    if(samples != NULL) {
        for(x = 0; x < nlines; x++) {
            if(samples[x].correlations != NULL)
                free(samples[x].correlations);
        }
        free(samples);
    }
}

ahp_xc_packet *ahp_xc_alloc_packet()
{
    ahp_xc_packet *packet = (ahp_xc_packet*)malloc(sizeof(ahp_xc_packet));
    packet->bps = (unsigned long)ahp_xc_get_bps();
    packet->tau = (unsigned long)((1000000000000<<ahp_xc_get_frequency_divider())/ahp_xc_get_frequency());
    packet->n_lines = (unsigned long)ahp_xc_get_nlines();
    packet->n_baselines = (unsigned long)ahp_xc_get_nbaselines();
    packet->counts = (unsigned long*)malloc(packet->n_lines*sizeof(unsigned long));
    packet->autocorrelations = ahp_xc_alloc_samples((unsigned long)ahp_xc_get_nlines(), (unsigned long)ahp_xc_get_autocorrelator_lagsize());
    packet->crosscorrelations = ahp_xc_alloc_samples((unsigned long)ahp_xc_get_nbaselines(), (unsigned long)ahp_xc_get_crosscorrelator_lagsize()*2-1);
    return packet;
}

void ahp_xc_free_packet(ahp_xc_packet *packet)
{
    if(packet != NULL) {
        if(packet->counts != NULL)
            free(packet->counts);
        ahp_xc_free_samples((unsigned long)ahp_xc_get_nlines(), packet->autocorrelations);
        ahp_xc_free_samples((unsigned long)ahp_xc_get_nbaselines(), packet->crosscorrelations);
        free(packet);
    }
}

void ahp_xc_start_crosscorrelation_scan(unsigned int index, off_t start)
{
    ahp_xc_set_capture_flag(CAP_ENABLE);
    ahp_xc_set_lag_cross(index, start);
    ahp_xc_set_test(index, SCAN_CROSS);
}

void ahp_xc_end_crosscorrelation_scan(unsigned int index)
{
    ahp_xc_clear_test(index, SCAN_CROSS);
    grab_last_packet();
    ahp_xc_clear_capture_flag(CAP_ENABLE);
}

int ahp_xc_scan_crosscorrelations(unsigned int index1, unsigned int index2, ahp_xc_sample **crosscorrelations, off_t start1, off_t start2, size_t size, int *interrupt, double *percent)
{
    int r = -1, y;
    unsigned int n = ahp_xc_get_bps()/4;
    *crosscorrelations = NULL;
    ahp_xc_sample *correlations = ahp_xc_alloc_samples((unsigned int)ahp_xc_get_delaysize(), (unsigned int)ahp_xc_get_autocorrelator_lagsize());
    char* sample = (char*)malloc((unsigned int)n+1);
    sample[n] = 0;
    (*percent) = 0;
    r++;
    start1 = (start1 > ahp_xc_get_delaysize()-2 ? start1 : (off_t)ahp_xc_get_delaysize()-2);
    start2 = (start2 > ahp_xc_get_delaysize()-2 ? start2 : (off_t)ahp_xc_get_delaysize()-2);
    size = (size < 5 ? 5 : size);
    ahp_xc_set_lag_cross(index2, (int)start2);
    ahp_xc_set_lag_auto(index1, 0);
    ahp_xc_set_lag_auto(index2, 0);
    ahp_xc_start_crosscorrelation_scan(index1, start1);
    size_t i = size/2;
    while(i > 0) {
        if((*interrupt) == 1)
            break;
        char* data = grab_next_valid_packet();
        if(!data)
            continue;
        char *packet = data;
        packet += 16;
        memcpy(sample, &packet[n*index1], (unsigned int)n);
        unsigned long counts = strtoul(sample, NULL, 16);
        memcpy(sample, &packet[n*index2], (unsigned int)n);
        counts += strtoul(sample, NULL, 16);
        counts /= 2;
        packet += n*(ahp_xc_get_nlines()+ahp_xc_get_autocorrelator_lagsize()*ahp_xc_get_nlines());
        unsigned int idx1 = (index1 < index2 ? index1 : index2);
        unsigned int idx2 = (index1 > index2 ? index1 : index2);
        packet += n*ahp_xc_get_crosscorrelator_lagsize()*((idx1*(ahp_xc_get_nlines()*2-idx1-1))/2+idx2-idx1-1);
        for(y = 0; y < ahp_xc_get_crosscorrelator_lagsize()*2-1; y++) {
            memcpy(sample, packet, (unsigned int)n);
            correlations[i].correlations[y].counts = counts;
            correlations[i].correlations[y].correlations = strtoul(sample, NULL, 16);
            correlations[i].correlations[y].coherence = (double)correlations[i].correlations[y].correlations / (double)correlations[i].correlations[y].counts;
            packet += n;
        }
        (*percent) += 50.0 / size;
        i++;
        r++;
        free(data);
    }
    ahp_xc_end_crosscorrelation_scan(index1);
    ahp_xc_set_lag_cross(index1, (int)start1);
    ahp_xc_set_lag_auto(index1, 0);
    ahp_xc_set_lag_auto(index2, 0);
    ahp_xc_start_crosscorrelation_scan(index2, start2);
    i = size/2;
    while(i < size) {
        if((*interrupt) == 1)
            break;
        char* data = grab_next_valid_packet();
        if(!data)
            continue;
        char *packet = data;
        packet += 16;
        memcpy(sample, &packet[n*index1], (unsigned int)n);
        unsigned long counts = strtoul(sample, NULL, 16);
        memcpy(sample, &packet[n*index2], (unsigned int)n);
        counts += strtoul(sample, NULL, 16);
        counts /= 2;
        packet += n*(ahp_xc_get_nlines()+ahp_xc_get_autocorrelator_lagsize()*ahp_xc_get_nlines());
        unsigned int idx1 = (index1 < index2 ? index1 : index2);
        unsigned int idx2 = (index1 > index2 ? index1 : index2);
        packet += n*ahp_xc_get_crosscorrelator_lagsize()*((idx1*(ahp_xc_get_nlines()*2-idx1-1))/2+idx2-idx1-1);
        for(y = 0; y < ahp_xc_get_crosscorrelator_lagsize()*2-1; y++) {
            memcpy(sample, packet, (unsigned int)n);
            correlations[i].correlations[y].counts = counts;
            correlations[i].correlations[y].correlations = strtoul(sample, NULL, 16);
            correlations[i].correlations[y].coherence = (double)correlations[i].correlations[y].correlations / (double)correlations[i].correlations[y].counts;
            packet += n;
        }
        (*percent) += 50.0 / size;
        i++;
        r++;
        free(data);
    }
    ahp_xc_end_crosscorrelation_scan(index2);
    free(sample);
    *crosscorrelations = correlations;
    return r;
}

void ahp_xc_start_autocorrelation_scan(unsigned int index, off_t start)
{
    ahp_xc_set_capture_flag(CAP_ENABLE);
    ahp_xc_set_lag_auto(index, start);
    ahp_xc_set_test(index, SCAN_AUTO);
}

void ahp_xc_end_autocorrelation_scan(unsigned int index)
{
    ahp_xc_clear_test(index, SCAN_AUTO);
    grab_last_packet();
    ahp_xc_clear_capture_flag(CAP_ENABLE);
}

int ahp_xc_scan_autocorrelations(unsigned int index, ahp_xc_sample **autocorrelations, off_t start, size_t len, int *interrupt, double *percent)
{
    int r = -1, y;
    unsigned int n = ahp_xc_get_bps()/4;
    int i = 0;
    *autocorrelations = NULL;
    ahp_xc_sample *correlations = ahp_xc_alloc_samples(len, (unsigned int)ahp_xc_get_autocorrelator_lagsize());
    char* sample = (char*)malloc((unsigned int)n+1);
    sample[n] = 0;
    (*percent) = 0;
    r++;
    start = (start < ahp_xc_get_delaysize()-2 ? start : (off_t)ahp_xc_get_delaysize()-2);
    off_t end = (start+(off_t)len < ahp_xc_get_delaysize() ? start+(off_t)len : (off_t)ahp_xc_get_delaysize()-1);
    ahp_xc_set_lag_cross(index, 0);
    ahp_xc_start_autocorrelation_scan(index, start);
    while(i < len) {
        if((*interrupt) || start >= end)
            break;
        char* data = grab_next_valid_packet();
        if(!data)
            continue;
        char *packet = data;
        packet += 16;
        memcpy(sample, &packet[index*n], (unsigned int)n);
        unsigned long counts = strtoul(sample, NULL, 16)|1;
        packet += n*ahp_xc_get_nlines();
        packet += n*index*ahp_xc_get_autocorrelator_lagsize();
        for(y = 0; y < ahp_xc_get_autocorrelator_lagsize(); y++) {
            memcpy(sample, packet, (unsigned int)n);
            correlations[i].correlations[y].counts = counts;
            correlations[i].correlations[y].correlations = strtoul(sample, NULL, 16);
            correlations[i].correlations[y].coherence = (double)correlations[i].correlations[y].correlations / (double)counts;
            packet += n;
        }
        (*percent) += 100.0 / len;
        i++;
        r++;
        free(data);
    }
    ahp_xc_end_autocorrelation_scan(index);
    free(sample);
    *autocorrelations = correlations;
    return r;
}

int ahp_xc_get_packet(ahp_xc_packet *packet)
{
    int ret = 1;
    int x = 0, y = 0, z = 0;
    int n = ahp_xc_get_bps()/4;
    char *sample = (char*)malloc((unsigned int)n+1);
    if(packet == NULL) {
        return -EINVAL;
    }
    char* data = grab_last_packet();
    if(!data){
        ret = -ENOENT;
        goto err_end;
    }
    char *buf = data;
    buf += 16;
    for(x = 0; x < ahp_xc_get_nlines(); x++) {
        sample[n] = 0;
        memcpy(sample, buf, (unsigned int)n);
        if(1<sscanf(sample, "%lX", &packet->counts[x])) {
            ret = -ENOENT;
            goto end;
        }
        packet->counts[x] = (packet->counts[x] == 0 ? 1 : packet->counts[x]);
        buf += n;
    }
    int idx = 0;
    for(x = 0; x < ahp_xc_get_nlines(); x++) {
        for(y = 0; y < ahp_xc_get_autocorrelator_lagsize(); y++) {
            sample[n] = 0;
            memcpy(sample, buf, (unsigned int)n);
            if(1<sscanf(sample, "%lX",  &packet->autocorrelations[x].correlations[y].correlations)) {
                ret = -ENOENT;
                goto end;
            }
            packet->autocorrelations[x].correlations[y].counts = packet->counts[x];
            packet->autocorrelations[x].correlations[y].coherence = (double)packet->autocorrelations[x].correlations[y].correlations/(double)packet->autocorrelations[x].correlations[y].counts;
            buf += n;
        }
    }
    idx = 0;
    for(x = 0; x < ahp_xc_get_nlines(); x++) {
        for(y = x+1; y < ahp_xc_get_nlines(); y++) {
            for(z = 0; z < (int)packet->crosscorrelations[idx].lag_size; z++) {
                sample[n] = 0;
                memcpy(sample, buf, (unsigned int)n);
                if(1<sscanf(sample, "%lX",  &packet->crosscorrelations[x].correlations[z].correlations)) {
                    ret = -ENOENT;
                    goto end;
                }
                packet->crosscorrelations[idx].correlations[z].counts = (packet->counts[x]+packet->counts[y])/2;
                packet->crosscorrelations[idx].correlations[z].coherence = (double)packet->crosscorrelations[idx].correlations[z].correlations/(double)packet->crosscorrelations[idx].correlations[z].counts;
                idx ++;
                buf += n;
            }
        }
    }
    free(data);
    ret = 0;
    goto end;
err_end:
    fprintf(stderr, "%s: %s\n", __func__, strerror(-ret));
end:
    free(sample);
    return ret;
}

int ahp_xc_get_properties()
{
    char *data = NULL;
    ssize_t n_read;
    int ntries = 4096;
    ahp_xc_clear_capture_flag(CAP_ENABLE);
    ahp_xc_set_capture_flag(CAP_ENABLE);
    while(ntries-- > 0) {
        data = grab_next_valid_packet();
        if(data)
            break;
    }
    ahp_xc_clear_capture_flag(CAP_ENABLE);
    if(ntries < 0 || data == NULL)
        return -EBUSY;
    unsigned int _bps, _nlines, _delaysize, _auto_lagsize, _cross_lagsize, _flags, _tau;
    n_read = sscanf((char*)data, "%02X%02X%03X%02X%02X%01X%04X", &_bps, &_nlines, &_delaysize, &_auto_lagsize, &_cross_lagsize, &_flags, &_tau);
    if(n_read != 7)
        return -EINVAL;
    strncpy(ahp_xc_header, data, 16);
    free(data);
    ahp_xc_bps = _bps;
    ahp_xc_nlines = _nlines+1;
    ahp_xc_nbaselines = ahp_xc_nlines*(ahp_xc_nlines-1)/2;
    ahp_xc_delaysize = _delaysize;
    ahp_xc_auto_lagsize = _auto_lagsize+1;
    ahp_xc_cross_lagsize = _cross_lagsize+1;
    ahp_xc_flags = _flags;
    ahp_xc_packetsize = (ahp_xc_get_nlines()+ahp_xc_get_autocorrelator_lagsize()*ahp_xc_get_nlines()+(ahp_xc_get_crosscorrelator_lagsize()*2-1)*ahp_xc_get_nbaselines())*ahp_xc_get_bps()/4+16+1;
    ahp_xc_frequency = (unsigned int)((long)1000000000000/(long)(!_tau?1:_tau));
    if(ahp_xc_test)
        free(ahp_xc_test);
    ahp_xc_test = (unsigned char*)malloc(ahp_xc_nlines);
    memset(ahp_xc_test, 0, ahp_xc_nlines);
    if(ahp_xc_leds)
        free(ahp_xc_leds);
    ahp_xc_leds = (unsigned char*)malloc(ahp_xc_nlines);
    memset(ahp_xc_leds, 0, ahp_xc_nlines);
    return 0;
}

int ahp_xc_set_capture_flag(xc_capture_flags flag)
{
    ahp_xc_capture_flags |= (1 << flag);
    RS232_flushRX();
    return (int)ahp_xc_send_command(ENABLE_CAPTURE, (unsigned char)ahp_xc_capture_flags);
}

int ahp_xc_clear_capture_flag(xc_capture_flags flag)
{
    ahp_xc_capture_flags &= ~(1 << flag);
    return (int)ahp_xc_send_command(ENABLE_CAPTURE, (unsigned char)ahp_xc_capture_flags);
}

void ahp_xc_set_baudrate(baud_rate rate)
{
    ahp_xc_rate = rate;
    ahp_xc_send_command(SET_BAUD_RATE, (unsigned char)rate);
    RS232_CloseComport();
    RS232_OpenComport(ahp_xc_comport);
    RS232_SetupPort(XC_BASE_RATE<<((int)ahp_xc_rate), "8N2", 0);
}

unsigned char ahp_xc_get_test(unsigned int index)
{
    return ahp_xc_test[index];
}

unsigned char ahp_xc_get_leds(unsigned int index)
{
    return ahp_xc_leds[index];
}

void ahp_xc_set_leds(unsigned int index, int leds)
{
    ahp_xc_leds[index] = (unsigned char)leds;
    ahp_xc_select_input(index);
    ahp_xc_send_command(SET_LEDS, (unsigned char)(leds&0xf));
}

void ahp_xc_set_lag_cross(unsigned int index, off_t value)
{
    ahp_xc_select_input(index);
    int idx = 0;
    ahp_xc_send_command(SET_DELAY, (unsigned char)((idx<<4)|(value&0x7)));
    idx++;
    value >>= 3;
    ahp_xc_send_command(SET_DELAY, (unsigned char)((idx<<4)|(value&0x7)));
    idx++;
    value >>= 3;
    ahp_xc_send_command(SET_DELAY, (unsigned char)((idx<<4)|(value&0x7)));
    idx++;
    value >>= 3;
    ahp_xc_send_command(SET_DELAY, (unsigned char)((idx<<4)|(value&0x7)));
    grab_last_packet();
}

void ahp_xc_set_lag_auto(unsigned int index, off_t value)
{
    ahp_xc_select_input(index);
    int idx = 0;
    ahp_xc_send_command(SET_DELAY, (unsigned char)((idx++<<4)|0x8|(value&0x7)));
    value >>= 3;
    ahp_xc_send_command(SET_DELAY, (unsigned char)((idx++<<4)|0x8|(value&0x7)));
    value >>= 3;
    ahp_xc_send_command(SET_DELAY, (unsigned char)((idx++<<4)|0x8|(value&0x7)));
    value >>= 3;
    ahp_xc_send_command(SET_DELAY, (unsigned char)((idx++<<4)|0x8|(value&0x7)));
    grab_last_packet();
}

void ahp_xc_set_frequency_divider(unsigned char value)
{
    value = (unsigned char)(value < 0xf ? value : 0xf);
    ahp_xc_send_command(SET_FREQ_DIV, value);
    ahp_xc_frequency_divider = value;
}

void ahp_xc_set_voltage(unsigned int index, unsigned char value)
{
    ahp_xc_select_input(index);
    value = (unsigned char)(value < 0xff ? value : 0xff);
    int idx = 0;
    ahp_xc_send_command((xc_cmd)(SET_VOLTAGE), (unsigned char)((idx++<<2)|(value&0x3)));
    value >>= 2;
    ahp_xc_send_command((xc_cmd)(SET_VOLTAGE), (unsigned char)((idx++<<2)|(value&0x3)));
    value >>= 2;
    ahp_xc_send_command((xc_cmd)(SET_VOLTAGE), (unsigned char)((idx++<<2)|(value&0x3)));
    value >>= 2;
    ahp_xc_send_command((xc_cmd)(SET_VOLTAGE), (unsigned char)((idx++<<2)|(value&0x3)));
    value >>= 2;
    ahp_xc_voltage = value;
}

void ahp_xc_set_test(unsigned int index, xc_test value)
{
    ahp_xc_select_input(index);
    ahp_xc_test[index] |= value;
    ahp_xc_send_command(ENABLE_TEST, ahp_xc_test[index]);
}

void ahp_xc_clear_test(unsigned int index, xc_test value)
{
    ahp_xc_select_input(index);
    ahp_xc_test[index] &= ~value;
    ahp_xc_send_command(ENABLE_TEST, ahp_xc_test[index]);
}

 ssize_t ahp_xc_send_command(xc_cmd c, unsigned char value)
{
    RS232_flushTX();
    return RS232_SendByte((unsigned char)(c|(((value<<4)|(value>>4))&0xf3)));
}