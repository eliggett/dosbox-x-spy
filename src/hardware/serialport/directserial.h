/*
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


// include guard
#ifndef DOSBOX_DIRECTSERIAL_WIN32_H
#define DOSBOX_DIRECTSERIAL_WIN32_H

#include "dosbox.h"

#if C_DIRECTSERIAL

#define DIRECTSERIAL_AVAILIBLE
#include "serialport.h"

#include "libserial.h"
#include "pic.h"
#include <deque>
#include <vector>
#include <utility>

class CDirectSerial : public CSerial {
public:
	CDirectSerial(Bitu id, CommandLine* cmd);
	~CDirectSerial();

	void updatePortConfig(double baudrate, uint8_t lcr) override;
	void updateMSR() override;
	void transmitByte(uint8_t val, bool first) override;
	void setBreak(bool value) override;
	
	void setRTSDTR(bool rts, bool dtr) override;
	void setRTS(bool val) override;
	void setDTR(bool val) override;
	void handleUpperEvent(uint16_t type) override;

private:
	COMPORT comport;

	Bitu rx_state = 0;
#define D_RX_IDLE		0
#define D_RX_WAIT		1
#define D_RX_BLOCKED	2
#define D_RX_FASTWAIT	3

	Bitu rx_retry;		// counter of retries (every millisecond)
	Bitu rx_retry_max;	// how many POLL_EVENTS to wait before causing
						// an overrun error.
	bool doReceive();

	// rib:1 - a Motorola RIB cable: DTR is the bus BUSY output and BUSY comes
	// back on CTS/RI. Programs like Motorola RSS drop DTR and expect RI to
	// follow within about a millisecond, faster than USB adapters report it.
	bool rib = false;
	bool ribDebug = false, ribDbgHw = false, ribDbgBusy = false;	// MOT_RIBDEBUG
	bool ribDtr = false;
	double ribHoldoff = 30.0;		// ms to ignore the hardware BUSY after a DTR change
	pic_tickindex_t ribDtrChanged = 0;
	// After we drop DTR the hardware BUSY lags for a few ms; once it reads
	// clear, BUSY is another device (a radio in test mode talks on its own
	// a few ms after a command), so the hold-off ends there.
	bool ribHwSettled = true;
	bool ribHwBusy = false;			// hardware BUSY seen since DTR went up
	pic_tickindex_t ribForeignRx = -1000.0;	// last byte from another device
	bool ribBusyOut();
	void ribSetDTR(bool val);
	bool ribPortDtr = false;		// the port's DTR (BUSY out)
	pic_tickindex_t ribTxDone = 0;	// when the bytes sent will have left the port
	void ribPortDtrDown();
	// Bytes another device sent while the program wasn't listening; dropped
	// when the program starts its next message, as a UART's overrun would.
	void ribDrainRx();
	// ribecho (default on with rib:1): the bus echoes every byte, and RSS
	// allows ~20 ms for it, less than a USB adapter's round trip may take.
	// Echo locally at once and drop the hardware copy when it arrives; a
	// hardware byte that differs is passed on, so collisions still show.
	bool ribEcho = true;
	std::deque<uint8_t> ribLocalEcho;
	std::deque<std::pair<uint8_t, pic_tickindex_t>> ribHwEcho;
	void ribDeliverEcho();
	// With local echo, bytes wait in ribTx until our BUSY is on the bus,
	// then (ribburst:1, the default) go to the port 5 at a time, one SB9600
	// message: the radio drops a message whose bytes are more than ~2 ms
	// apart, and one write per byte through a USB adapter goes out on its
	// 1 ms frame schedule, which can spread them that far. ribburst:0
	// sends each byte at once.
	bool ribBurst = true;
	std::vector<uint8_t> ribTx;
	pic_tickindex_t ribTxLast = 0;
	void ribFlush();

#if SERIAL_DEBUG
	bool dbgmsg_poll_block = false;
	bool dbgmsg_rx_block = false;
#endif

};

#endif	// C_DIRECTSERIAL
#endif	// include guard
