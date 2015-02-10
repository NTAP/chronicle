// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#ifndef TCP_STREAM_NAVIGATOR_H
#define TCP_STREAM_NAVIGATOR_H

#include <inttypes.h>

class PduDescriptor;
class PacketDescriptor;

/**
 * A class providing basic primitives to parse uint32_t values/byte streams and
 * to skip bytes across a PDU consisting of multiple packets.
 */
class TcpStreamNavigator {
	public:
		TcpStreamNavigator();
		~TcpStreamNavigator();
		/**
		 * initializes a TCP stream navigator with a packet and a starting offset
		 * @param[in] pktDesc The packet being parsed
		 * @param[in] index The starting offset for stream navigator
		 */
		bool init(PacketDescriptor *pktDesc, uint16_t index);
		/**
		 * initializes a TCP stream navigator with a PDU and a starting offset
		 * @param[in] pduDesc The PDU being parsed
		 * @param[in] index The starting offset for stream navigator
		 * @param[in] pktDesc In case parser gets initialized to not the beginning of PDU
		 */
		bool init(PduDescriptor *pduDesc, uint16_t index, 
			PacketDescriptor *pktDesc = NULL);
		/**
		 * retrieves a uint32_t value in host representation and advances the index
		 * @param[in] uint32 The address where the retreived value is written
		 * @param[in] scanMode Set to true when visit counts remain unchanged
		 * @returns true when successful and false otherwise
		 */
		bool getUint32Packet(uint32_t *uint32, bool scanMode = false);
		/**
		 * retrieves a uint32_t value in host representation and advances the index
		 * @param[in] uint32 The address where the retreived value is written
		 * @returns true when successful and false otherwise
		 */
		bool getUint32Pdu(uint32_t *uint32);
		/**
		 * retrieves a uint64_t value in host representation and advances the index
		 * @param[in] uint64 The address where the retreived value is written
		 * @param[in] scanMode Set to true when visit counts remain unchanged
		 * @returns true when successful and false otherwise
		 */
		bool getUint64Packet(uint64_t *uint64, bool scanMode = false);
		/**
		 * retrieves a uint64_t value in host representation and advances the index
		 * @param[in] uint64 The address where the retreived value is written
		 * @returns true when successful and false otherwise
		 */
		bool getUint64Pdu(uint64_t *uint64);
		/**
		 * retrieves an xdr_string and advances the index
		 * @param[in] str The address where the retreived string is written
		 * @param[in] maxStringLen The max length of the string
		 * @returns true when successful and false otherwise
		 */
		bool getStringPdu(unsigned char *str, uint32_t maxStringLen);
		/**
		 * retrieves a byte stream of certain length and advances the index
		 * @param[in] bytes The address where the retreived bytes are written
		 * @param[in] numBytes The number of bytes being retrieved
		 * @param[in] scanMode Set to true when visit counts remain unchanged
		 * @returns true when successful and false otherwise
		 */
		bool getBytesPacket(unsigned char *bytes, uint32_t numBytes, 
			bool scan = false);
		/**
		 * retrieves a byte stream of certain length and advances the index
		 * @param[in] bytes The address where the retreived bytes are written
		 * @param[in] numBytes The number of bytes being retrieved
		 * @returns true when successful and false otherwise
		 */
		bool getBytesPdu(unsigned char *bytes, uint32_t numBytes);
		/**
		 * skips so many bytes in the flow stream
		 * @param[in] numBytes The number of bytes being skipped
		 * @param[in] scanMode Set to true when visit counts remain unchanged
		 * @returns true when successful and false otherwise
		 */
		bool skipBytesPacket(uint32_t numBytes, bool scanMode = false);
		/**
		 * skips so many bytes in the PDU (across packets)
		 * @param[in] numBytes The number of bytes being skipped
		 * @returns true when successful and false otherwise
		 */
		bool skipBytesPdu(uint32_t numBytes);
		/**
		 * goes back so many bytes in the PDU (across packets)
		 * @param[in] numBytes The number of bytes going back
		 * @param[in] scanMode Set to true when the current packet is set to unscanned
		 * @returns true when successful and false otherwise
		 */
		bool goBackBytesPacket(uint32_t numBytes, bool scanMode = false);
		/**
		 * advances the parser to the next packet in the flow stream
		 * @param[in] scanMode Set to true when visit counts remain unchanged
		 * @returns true when the next packet is in-order
		 */
		bool advanceToNextPacket(bool scanMode = false);
		/**
		 * advances the parser from one packet in the PDU to the next packet
		 */
		void advanceToNextPduPacket();
		/**
		 * takes the parser so many bytes back in the flow stream
		 * @returns true when the operation was successful
		 */
		bool goBackToPrevPacket();
		/**
		 * @returns the current packet descriptor within the parser
		 */
		PacketDescriptor *getPacketDesc() { return pktDesc; }
		/**
		 * @returns the position of index within the current packet being parsed
		 */
		uint16_t getIndex() { return index; }
		/**
		 * @returns the number of bytes parsed thus far
		 */
		uint16_t getParsedBytes() { return parsedBytes; }
		/**
		 * @returns the number of packets parsed thus far
		 */
		uint8_t getNumParsedPackets() { return parsedPackets; }

	protected:
		/// the PDU descriptor being parsed
		PduDescriptor *pduDesc;
		/// the packet in the PDU being parsed
		PacketDescriptor *pktDesc;
		/// the packet in the PDU being parsed
		PacketDescriptor *startingPktDesc;
		/// the address of the packet buffer
		unsigned char *ethFrame;
		/// the number of parsed bytes
		uint16_t index;
		/// the length of the packet
		uint16_t packetLen;
		/// the number of parsed bytes
		uint16_t parsedBytes;
		/// the number of packets traversed
		uint8_t parsedPackets;
};

#endif // TCP_STREAM_NAVIGATOR_H
