/*
 * videoencryption.c
 *
 *  Created on: May 29, 2014
 *      Author: fabio
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <openssl/aes.h>
//#include <openssl/rand.h>
#include <openssl/evp.h>

#include "encryptionlibrary.h"

// main entrypoint
int main(int argc, char **argv) {

	int H264_IFRAME_START_CODE_LEN = 3;

	char *TS_FILE = "/home/fabio/workspace2/DemoTS/file34-10MB.mpg";
	char *TS_OUTPUT_FILE = "/home/fabio/workspace2/DemoTS/encX-file34-10MB.mpg";

	unsigned char aes_key[16] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
			0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F }; // 128:8=16

	unsigned char aes_iv[16] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
			0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F };

	const int VIDEO_PID = 0x31; // depending on the stream used

	/* initialization */
	static int TS_PACKET_LEN = 188;
	unsigned char *input_buffer;
	unsigned char *aes_output;
	int buffer_size; // size of the buffer ( size of the input file)
	int out_len;
	/*------------------------------*/

	// load file into memory
	buffer_size = loadFile(TS_FILE, &input_buffer);
	aes_output = malloc(buffer_size * sizeof(char));
	memset(aes_output, 0, buffer_size * sizeof(char));
	//hex_print(aes_input, 32);

	int index = 0, pid, h264Index, inIframe = 0, lastPacketIsIFrame = 0,
			pes_packet_length;
	unsigned char* ts_packet;
	unsigned char* pes_packet;
	unsigned char* aes_input;

	int interations = buffer_size / TS_PACKET_LEN;
	printf("-> %d iterations", interations);
	while (index < interations) {
//	while (index < 15) {
		ts_packet = input_buffer + (index * TS_PACKET_LEN);

		// 1) check we have the sync byte
		if (ts_packet[0] == 0x47) {

			// 2) check it's a video packet
			pid = ts_packet[2] + ((ts_packet[+1] & 0x1F) << 8);
			if (pid == VIDEO_PID) {
//				Log("Found a Video Pid");

				// 3) check Unit Start Indicator (USI) is 1
				if ((ts_packet[1] & 0x40) == 0x40) {
//					Log("Unit start Indicator :: Start of Video frame (I/P/B)");

					// 4) Check the TS packet has PAYLOAD B[3]&0x10 == 1.
					if ((ts_packet[3] & 0x10) == 0x10) {
//						Log("Found payload");
						int AFC_len = 0;

						// 4a) Se ha il payload, controllo che ci sia Adaptation Field Control (AFC)
						// B[3]&0x20 == 0x20
						if ((ts_packet[3] & 0x20) == 0x20) {
							// 4b) Se ha AFC, calcolo la sua lunghezza
							// AFC_len=B[4] altrimenti AFC_len=0
							AFC_len = ts_packet[4];
//							Log("Found AFC");
						} else {
//							Log("AFC not Found");
						}
//						printf("AFC len=%d", AFC_len);

						pes_packet = ts_packet + 4 + AFC_len;
						pes_packet_length = TS_PACKET_LEN - (4 + AFC_len);

						// getH264StartCodeIndex( bufer, startIndex, buffer_len)
						h264Index = getH264StartCodeIndex(pes_packet, 4,
								pes_packet_length);

						if (h264Index > -1) {
//							printf("trovato h264 payload a h264Index=%d",
//									h264Index);

							// 5) per ogni sequenza S, prendo il
							// NAL_unit_type S[3]
							int NAL_unit_type = pes_packet[h264Index
									+ H264_IFRAME_START_CODE_LEN + 1] & 0x1F;

//							printf("\n++++++++++++++++++++++++++++++++");
//							printf("\n-------indexh264=%d", h264Index);
//							hex_print(pes_packet, pes_packet_length);
//							printf("\n-------\n");
//							printf("\nNAL_unit_type=0x%02x", NAL_unit_type);
//							printf("\nNAL_unit index=%d",
//									(h264Index + H264_IFRAME_START_CODE_LEN + 1));
//							printf("\n------------------------------");

							// 6b) se e' 0x09 e' un Access Delimiter
							if (NAL_unit_type == 0x09) {
//								Log("Found a Access Delimiter");

								// 6c) Poi prendo il prox byte
								int primary_pic_type = pes_packet[h264Index
										+ H264_IFRAME_START_CODE_LEN + 1]
										& 0x60;
								primary_pic_type = primary_pic_type >> 5;
								switch (primary_pic_type) {
								case 0:
//									Log("I-Frame");
									inIframe = 1;
									break;
								case 1:
//									Log("P-Frame");
									inIframe = 0;
									break;
								case 2:
//									Log("B-Frame");
									inIframe = 0;
									break;
								default:
									Log("ERROR: frame undefined");
									return -1;
								}

							}
						} else {
//							Log("NOT a Access Delimiter");
						}

					} else {
//						Log("NO h264 payload");
					}

				} else {
//					Log("payload not Found");
				}
			} else {
//				Log("Not a a Video Pid");
			}

			if (inIframe == 1) {
//				Log("setTransportScramblingControl");
				unsigned char transport_scrambling_control = 0x00;
				if (lastPacketIsIFrame == 1) {
					transport_scrambling_control = 0x02; // [1][0]
				} else {
					// start of I-Frame !!!!
					transport_scrambling_control = 0x03; // [1][1]
				}
				setTransportScramblingControl(ts_packet,
						transport_scrambling_control);

				// +++++ encryption +++
				aes_input = ts_packet + 12;
				// Initialize OpenSSL
				EVP_CIPHER_CTX ctx;
				EVP_CIPHER_CTX_init(&ctx);
				EVP_CIPHER_CTX_set_padding(&ctx, 0); //0 for no padding, 1 for padding  // ret ==1 here
				EVP_EncryptInit_ex(&ctx, EVP_aes_128_cbc(), NULL, aes_key, aes_iv);
				EVP_EncryptUpdate(&ctx, aes_input, &out_len, aes_input, 176);
				// ----- encryption ---

				lastPacketIsIFrame = 1;
			} else {
				lastPacketIsIFrame = 0;
			}
			index++;
		} else {
			Log("ERROR: missed a sync byte. not a TS stream");
			return -1;
		}

	} // while()

	saveFile(TS_OUTPUT_FILE, input_buffer, buffer_size);

	return 0;
}
