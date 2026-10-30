/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

// serialN=rib port:7600 [host:127.0.0.1]
//
// A Motorola RIB (radio interface box) on an SB9600 bus, with the radio on
// the far end of a TCP connection. The bus is modelled here so its timing
// doesn't depend on the network: every transmitted byte echoes back at once,
// and DTR (the PC's BUSY output) shows up on RI and CTS immediately.
//
// Wire format, both directions, 2-byte records:
//   'D' byte   a data byte (to the radio: sent by the PC; from it: radio sends)
//   'B' 0|1    BUSY (to the radio: the PC's DTR; from it: the radio's BUSY)
//   'R' n      line rate changed to n*100 baud (to the radio only)

#ifndef INCLUDEGUARD_SERIALRIB_H
#define INCLUDEGUARD_SERIALRIB_H

#include <deque>
#include "serialport.h"

class CSerialRIB : public CSerial {
public:
	CSerialRIB(Bitu id, CommandLine* cmd);
	virtual ~CSerialRIB();

	void setRTSDTR(bool rts, bool dtr) override;
	void setRTS(bool val) override;
	void setDTR(bool val) override;

	void updatePortConfig(double baudrate, uint8_t lcr) override;
	void updateMSR() override;
	void transmitByte(uint8_t val, bool first) override;
	void setBreak(bool value) override;
	void handleUpperEvent(uint16_t type) override;

private:
	void sendRecord(uint8_t kind, uint8_t value);
	void pollSocket();
	void updateBusy();
	void kickRx();

	int sock = -1;
	bool pcBusy = false;
	bool radioBusy = false;
	bool rxScheduled = false;
	uint8_t partial[2] = {0, 0};
	int partialLen = 0;
	std::deque<uint8_t> rxQueue;
};

#endif
