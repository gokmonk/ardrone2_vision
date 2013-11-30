
#include <stdint.h>
#include <string.h>
#include "udp/socket.h"

#include "rtp.h"

// http://www.ietf.org/rfc/rfc3550.txt

#define KJpegCh1ScanDataLen 32
#define KJpegCh2ScanDataLen 56

// RGB JPEG images as RTP payload - 64x48 pixel
char JpegScanDataCh2A[KJpegCh2ScanDataLen] =
{
    0xf8, 0xbe, 0x8a, 0x28, 0xaf, 0xe5, 0x33, 0xfd,
    0xfc, 0x0a, 0x28, 0xa2, 0x80, 0x0a, 0x28, 0xa2,
    0x80, 0x0a, 0x28, 0xa2, 0x80, 0x0a, 0x28, 0xa2,
    0x80, 0x0a, 0x28, 0xa2, 0x80, 0x0a, 0x28, 0xa2,
    0x80, 0x0a, 0x28, 0xa2, 0x80, 0x0a, 0x28, 0xa2,
    0x80, 0x0a, 0x28, 0xa2, 0x80, 0x0a, 0x28, 0xa2,
    0x80, 0x0a, 0x28, 0xa2, 0x80, 0x3f, 0xff, 0xd9
};
char JpegScanDataCh2B[KJpegCh2ScanDataLen] =
{
    0xf5, 0x8a, 0x28, 0xa2, 0xbf, 0xca, 0xf3, 0xfc,
    0x53, 0x0a, 0x28, 0xa2, 0x80, 0x0a, 0x28, 0xa2,
    0x80, 0x0a, 0x28, 0xa2, 0x80, 0x0a, 0x28, 0xa2,
    0x80, 0x0a, 0x28, 0xa2, 0x80, 0x0a, 0x28, 0xa2,
    0x80, 0x0a, 0x28, 0xa2, 0x80, 0x0a, 0x28, 0xa2,
    0x80, 0x0a, 0x28, 0xa2, 0x80, 0x0a, 0x28, 0xa2,
    0x80, 0x0a, 0x28, 0xa2, 0x80, 0x3f, 0xff, 0xd9
};

void send_rtp_packet(struct UdpSocket *sock, char * Jpeg, int JpegLen, uint32_t m_SequenceNumber, uint32_t m_Timestamp, uint32_t m_offset, uint8_t marker_bit, int w, int h);

void send_rtp_frame(struct UdpSocket *sock, char * Jpeg, uint32_t JpegLen, int w, int h)
{
  static uint32_t framecounter = 0;
  static uint32_t timecounter = 0;
  uint32_t offset = 0;

#define MAX_PACKET_SIZE 512

  // Split frame into packets
  for (;JpegLen > 0;)
  {
    uint32_t len = MAX_PACKET_SIZE;
    uint8_t lastpacket = 0;

    if (JpegLen <= len)
    {
      lastpacket = 1;
      len = JpegLen;
    }

    send_rtp_packet(sock, Jpeg,len,framecounter, timecounter, offset, lastpacket, w, h);

    JpegLen   -= len;
    Jpeg      += len;
    offset    += len;
  }

  framecounter++;
  // timestamp = 1 / 90 000 seconds
  timecounter+=3600;
}

/*
 * The RTP timestamp is in units of 90000Hz. The same timestamp MUST
 appear in each fragment of a given frame. The RTP marker bit MUST be
 set in the last packet of a frame.
 *
 */

void send_rtp_packet(struct UdpSocket *sock, char * Jpeg, int JpegLen, uint32_t m_SequenceNumber, uint32_t m_Timestamp, uint32_t m_offset, uint8_t marker_bit, int w, int h)
{

#define KRtpHeaderSize 12           // size of the RTP header
#define KJpegHeaderSize 8           // size of the special JPEG payload header

  char        RtpBuf[2048];
  int         RtpPacketSize = JpegLen + KRtpHeaderSize + KJpegHeaderSize;

  memset(RtpBuf,0x00,sizeof(RtpBuf));

  /*
   The RTP header has the following format:

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |V=2|P|X|  CC   |M|     PT      |       sequence number         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                           timestamp                           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |           synchronization source (SSRC) identifier            |
   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
   |            contributing source (CSRC) identifiers             |
   |                             ....                              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   * */

  // Prepare the 12 byte RTP header
  RtpBuf[0]  = 0x80;                               // RTP version
  RtpBuf[1]  = 0x1a + (marker_bit<<7);             // JPEG payload (26) and marker bit
  RtpBuf[2]  = m_SequenceNumber & 0x0FF;           // each packet is counted with a sequence counter
  RtpBuf[3]  = m_SequenceNumber >> 8;
  RtpBuf[4]  = (m_Timestamp & 0xFF000000) >> 24;   // each image gets a timestamp
  RtpBuf[5]  = (m_Timestamp & 0x00FF0000) >> 16;
  RtpBuf[6]  = (m_Timestamp & 0x0000FF00) >> 8;
  RtpBuf[7]  = (m_Timestamp & 0x000000FF);
  RtpBuf[8]  = 0x13;                               // 4 byte SSRC (sychronization source identifier)
  RtpBuf[9]  = 0xf9;                               // we just an arbitrary number here to keep it simple
  RtpBuf[10] = 0x7e;
  RtpBuf[11] = 0x67;

  /* JPEG header", are as follows:
   *
   * http://tools.ietf.org/html/rfc2435

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   | Type-specific |              Fragment Offset                  |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |      Type     |       Q       |     Width     |     Height    |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   */

  // Prepare the 8 byte payload JPEG header
  RtpBuf[12] = 0x00;                               // type specific
  RtpBuf[13] = (m_offset & 0x00FF0000) >> 16;      // 3 byte fragmentation offset for fragmented images
  RtpBuf[14] = (m_offset & 0x0000FF00) >> 8;
  RtpBuf[15] = (m_offset & 0x000000FF);
  RtpBuf[16] = 0x01;                               // type: 0 422 or 1 421
  RtpBuf[17] = 0x5e;                               // quality scale factor
  RtpBuf[18] = w/8;                           // width  / 8 -> 48 pixel
  RtpBuf[19] = h/8;                           // height / 8 -> 32 pixel
  // append the JPEG scan data to the RTP buffer
  memcpy(&RtpBuf[20],Jpeg,JpegLen);

  udp_write(sock,RtpBuf,RtpPacketSize);
};