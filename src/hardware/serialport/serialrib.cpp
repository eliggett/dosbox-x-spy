/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include "dosbox.h"

#if !defined(WIN32)

#include "logging.h"
#include "serialport.h"
#include "serialrib.h"

#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/socket.h>

CSerialRIB::CSerialRIB(Bitu id, CommandLine* cmd) : CSerial(id, cmd) {
	InstallationSuccessful = false;

	std::string host = "127.0.0.1";
	std::string portstr;
	cmd->FindStringBegin("host:", host, false);
	if (!cmd->FindStringBegin("port:", portstr, false)) portstr = "7600";
	int port = atoi(portstr.c_str());

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		LOG_MSG("Serial%d: rib: socket() failed", (int)COMNUMBER);
		return;
	}
	struct sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_port = htons((uint16_t)port);
	if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1 ||
	    connect(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
		LOG_MSG("Serial%d: rib: cannot connect to %s:%d", (int)COMNUMBER, host.c_str(), port);
		close(sock);
		sock = -1;
		return;
	}
	int one = 1;
	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
	fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK);
	LOG_MSG("Serial%d: rib: connected to radio at %s:%d", (int)COMNUMBER, host.c_str(), port);

	CSerial::Init_Registers();
	setDSR(false);
	setCD(false);
	updateBusy();
	InstallationSuccessful = true;
	setEvent(SERIAL_POLLING_EVENT, 1);
}

CSerialRIB::~CSerialRIB() {
	removeEvent(SERIAL_POLLING_EVENT);
	removeEvent(SERIAL_RX_EVENT);
	removeEvent(SERIAL_TX_EVENT);
	removeEvent(SERIAL_THR_EVENT);
	if (sock >= 0) close(sock);
}

void CSerialRIB::sendRecord(uint8_t kind, uint8_t value) {
	if (sock < 0) return;
	uint8_t rec[2] = {kind, value};
	if (send(sock, rec, 2, MSG_NOSIGNAL) != 2)
		LOG_MSG("Serial%d: rib: send failed", (int)COMNUMBER);
}

void CSerialRIB::pollSocket() {
	if (sock < 0) return;
	uint8_t buf[256];
	for (;;) {
		ssize_t n = recv(sock, buf, sizeof(buf), 0);
		if (n == 0) {
			LOG_MSG("Serial%d: rib: radio disconnected", (int)COMNUMBER);
			close(sock);
			sock = -1;
			return;
		}
		if (n < 0) return; // EAGAIN: nothing waiting
		for (ssize_t i = 0; i < n; i++) {
			partial[partialLen++] = buf[i];
			if (partialLen < 2) continue;
			partialLen = 0;
			if (partial[0] == 'D') rxQueue.push_back(partial[1]);
			else if (partial[0] == 'B') {
				radioBusy = partial[1] != 0;
				updateBusy();
			}
		}
	}
}

// The bus BUSY line is wired-OR: either end can hold it. The RIB returns it
// on CTS, and the RSS cable ties CTS to RI.
void CSerialRIB::updateBusy() {
	bool busy = pcBusy || radioBusy;
	setRI(busy);
	setCTS(busy);
}

void CSerialRIB::kickRx() {
	if (!rxScheduled && !rxQueue.empty()) {
		rxScheduled = true;
		setEvent(SERIAL_RX_EVENT, bytetime);
	}
}

void CSerialRIB::handleUpperEvent(uint16_t type) {
	switch (type) {
	case SERIAL_POLLING_EVENT:
		setEvent(SERIAL_POLLING_EVENT, 1.0f);
		pollSocket();
		kickRx();
		break;
	case SERIAL_RX_EVENT:
		rxScheduled = false;
		if (!rxQueue.empty() && CanReceiveByte()) {
			receiveByte(rxQueue.front());
			rxQueue.pop_front();
		}
		kickRx();
		break;
	case SERIAL_THR_EVENT:
		ByteTransmitting();
		setEvent(SERIAL_TX_EVENT, bytetime);
		break;
	case SERIAL_TX_EVENT:
		ByteTransmitted();
		break;
	}
}

void CSerialRIB::updatePortConfig(double baudrate, uint8_t lcr) {
	(void)lcr;
	unsigned hundreds = (unsigned)(baudrate / 100.0 + 0.5);
	sendRecord('R', (uint8_t)(hundreds > 255 ? 255 : hundreds));
}

void CSerialRIB::updateMSR() {
}

// The bus loops every byte back to the sender; queue the echo so it lands
// one byte-time later, in order with anything the radio sends.
void CSerialRIB::transmitByte(uint8_t val, bool first) {
	sendRecord('D', val);
	rxQueue.push_back(val);
	kickRx();
	if (first) setEvent(SERIAL_THR_EVENT, bytetime / 8);
	else setEvent(SERIAL_TX_EVENT, bytetime);
}

void CSerialRIB::setBreak(bool value) {
	(void)value;
}

void CSerialRIB::setRTSDTR(bool rts, bool dtr) {
	setRTS(rts);
	setDTR(dtr);
}

void CSerialRIB::setRTS(bool val) {
	(void)val;
}

void CSerialRIB::setDTR(bool val) {
	if (val == pcBusy) return;
	pcBusy = val;
	sendRecord('B', val ? 1 : 0);
	updateBusy();
}

#endif
