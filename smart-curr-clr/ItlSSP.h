#pragma once
using namespace System;
using namespace System::Threading;
using namespace System::Text;
using namespace System::Collections::Generic;


namespace ItlSSPSystem
{
	typedef enum {
		cmdSync = 0x11,
		cmdGetFirmware = 0x20,
		cmdGetSerial = 0x0C,
		cmdGetIP = 0x16,
		cmdSetUpRequest = 0x05,
		cmdPoll = 0x07,
		cmdHoldBill = 0x18,
		cmdRejectBill = 0x08,
		cmdEnable = 0x0A,
		cmdDisable = 0x09,
		cmdCountryData = 0x5E,
	}ItlSSPCommand;


	typedef enum {
		reset = 0xF1,
		billRead = 0xEF,
		disabled = 0xE8,
		billCredit = 0xEE,
		billStacking = 0xCC,
		billStacked = 0xEB,
		billRejecting = 0xED,
		billRejected = 0xEC,
		inhibitsSet = 0xB5,
		cashboxRemoved = 0xE3,
		cashboxReplaced = 0xE4,
		safeJam = 0xEA,
		unsafeJam = 0xE9,
		billStackerFull = 0xE7,
		billPathOpen = 0xE0,
	}ItlSSPEvents;

	typedef enum {
		idle,
		connect,
		initialise,
		indexCurrencies,
		enable,
		run,
		holdBill,
	}SystemState;


	public ref class ItlValue{
	private:
	public:
		String^ countryCode = gcnew String("");
		int value = 0;
	};


	public ref class ItlCurrency {

	private:
	public:
		bool crcStatus;
		int inhibitStatus;
		String^ countryCode = gcnew String("");
		String^ version = gcnew String("");
		int numberOfDenominations = 0;

	};



	public ref class ItlDevice {
	private:

	public:
		String^ FirmwareVersion = gcnew String("");
		String^ IPAddress = gcnew String("");
		String^ SerailNumber = gcnew String("");
		int numberOfCurrencies;
		static List<ItlCurrency^>^ currencies;
		bool billInEscrow;
		ItlValue^ escrowBill;
		ItlValue^ creditBill;
		bool newBillCredit;
		

		ItlDevice() {
			numberOfCurrencies = 0;
			currencies = gcnew List<ItlCurrency^>();
			billInEscrow = false;
			escrowBill = gcnew ItlValue();
			newBillCredit = false;
			creditBill = gcnew ItlValue();
		}


	};



	private ref class ItlSSPPacket {

	private:

		const int BUFFER_SIZE = 256;
	public:

		ItlSSPPacket() {
			data = gcnew array<unsigned char>(BUFFER_SIZE);
			length = 0;
			index = 0;
		}
		array<unsigned char>^ data;
		int length;
		int index;

	};



	public ref class ItlSSP {

	private:

		static const unsigned char STX = 0x7F;
		static const unsigned char FIXED_PACKET_LENGTH = 5;

		static unsigned char seq;
		static unsigned char address;
		static Boolean stuffed;
		static ItlSystemPort^ sys;
		static ItlSSPPacket^ tx;
		static ItlSSPPacket^ rx;
		static Boolean _continue;
		static Boolean newResponse;


		static void ReadPort(void)
		{
			_continue = true;
			while (_continue) {
				Thread::Sleep(50);
				if (sys->PortIsOpen()) {
					int ret = sys->Read();
					ParseResponse(sys->GetBuffer(), ret);
				}
			}
		}

		static void ParseResponse(array<unsigned char>^ data, int length)
		{

			for (int i = 0; i < length; i++) {

				unsigned char dt = data[i];

				// start of packet
				if (dt == STX && rx->index == 0) {
					rx->data[rx->index++] = dt;
				}
				else {
					if (stuffed) {
						if (dt != STX) { // new packet start so reset packet
							rx->data[0] = STX;
							rx->data[1] = dt;
							rx->index = 2;
						}
						else { // byte is stuffed - extract byte
							rx->data[rx->index++] = dt;
							if (rx->index == 3) {
								rx->length = rx->data[2] + FIXED_PACKET_LENGTH;
							}
						}
						stuffed = false;
					}
					else {
						//repeated STX so falg for stuffed check on next byte
						if (dt == STX) {
							stuffed = true;
						}
						else {
							rx->data[rx->index++] = dt;
							// set full packet length when index is at set point
							if (rx->index == 3) {
								rx->length = rx->data[2] + FIXED_PACKET_LENGTH;
							}
						}
					}
				}

				// full packet recived
				if (rx->index > 3 && rx->index == rx->length) {
					// address response?
					if ((rx->data[1] & STX) == address) {
						//CRC test
						UInt16 crc = CalculateCRC(rx->data, 1, rx->length - 3);
						if ((crc & 0xFF) == rx->data[rx->length - 2] && ((crc >> 8) & 0xFF) == rx->data[rx->length - 1]) {
							if (showPackets) {
								String^ hex = BitConverter::ToString(rx->data, 0, rx->length);
								Console::WriteLine("Recieve: " + hex);
							}
						}

						// seq from slave should match seq form host
						unsigned char pktseq = (rx->data[1] & 0x80);
						if (seq == pktseq)
						{
							// toggle seq
							seq = (seq == 0x80) ? 0x00 : 0x80;
							newResponse = true;

						}

					}
				}

			}

		}


	public:

		SystemState state;
		static ItlDevice^ itlDevice;
		static bool showPackets;

		/**
		*@brief Class constructor
		*/
		ItlSSP(ItlSystemPort^ port)
		{
			sys = port;
			itlDevice = gcnew ItlDevice();
			tx = gcnew ItlSSPPacket();
			rx = gcnew ItlSSPPacket();

			seq = 0x80;
			address = 0;
			state = connect;
			showPackets = false;
			Thread^ td = gcnew Thread(gcnew ThreadStart(ItlSSP::ReadPort));
			td->Start();


		}
		~ItlSSP() {
			if (sys->PortIsOpen()) {
				sys->ClosePort();
			}
			newResponse = false;
			delete tx;
			delete rx;
			delete itlDevice;

		}


		bool OpenPort(String^ portName)
		{

			return sys->OpenPort(portName);

		}



		bool SSPCommand(ItlSSPCommand cmd, array<unsigned char>^ data, int length)
		{

			tx->length = 0;

			// for sync commands pre-set the seq
			if (cmd == cmdSync) {
				seq = 0x80;
			}

			tx->data[tx->length++] = 0x7F;
			tx->data[tx->length++] = address | seq;
			tx->data[tx->length++] = length + 1;
			tx->data[tx->length++] = (unsigned char)cmd;
			for (int i = 0; i < length; i++) {
				tx->data[tx->length++] = data[i];
			}
			// add crc
			UInt16 crc = CalculateCRC(tx->data, 1, tx->length - 1);
			tx->data[tx->length++] = (unsigned char)(crc & 0xFF);
			tx->data[tx->length++] = (unsigned char)((crc >> 8) & 0xFF);

			// byte stuff 
			array<unsigned char>^ tmpBuffer = gcnew array<unsigned char>(256);
			int j = 0;
			tmpBuffer[j++] = tx->data[0];
			for (int i = 1; i < tx->length; i++) {
				tmpBuffer[j] = tx->data[i];
				if (tx->data[i] == STX) {
					tmpBuffer[++j] = STX;
				}
				j++;
			}
			for (int i = 0; i < j; i++) {
				tx->data[i] = tmpBuffer[i];
			}
			tx->length = j;

			tx->index = 0;
			rx->index = 0;
			stuffed = false;
			newResponse = false;
			sys->WriteData(tx->data, tx->length);


			int retries = 0;
			int timer = 0;
			while (!newResponse && retries < 3) {
				// check for timeout retry
				if ((timer == 0 || timer >= 100)) {
					if (showPackets) {
						String^ hex = BitConverter::ToString(tx->data, 0, tx->length);
						Console::WriteLine("Transmit: " + hex);
					}
					timer = 0;
					retries++;
				}
				Thread::Sleep(10);
				timer++;
			}

			return newResponse;


		}

		/**
		*@brief
		*/
		static UInt16 CalculateCRC(array<unsigned char>^ data, int offset, int length)
		{

			int i, j;
			UInt16 crc = 0xFFFF;

			for (i = 0; i < length; ++i) {
				crc ^= (data[i + offset] << 8);
				for (j = 0; j < 8; ++j) {
					if (crc & 0x8000)
						crc = (crc << 1) ^ 0x8005;
					else
						crc <<= 1;
				}
			}
			return crc;

		}


		bool ParseSetUpData(void)
		{
			if (rx->data[4] != 0x0C) {
				Console::WriteLine("Device is not SMART Currency!");
				return false;
			}
			itlDevice->numberOfCurrencies = (int)rx->data[5] + ((int)rx->data[6] << 8);

			return true;
		}

		bool ParseFirmwareData(void)
		{
			itlDevice->FirmwareVersion = Encoding::ASCII->GetString(rx->data, 4, rx->data[2] - 1);
			return true;
		}

		bool ParseSerialNumberData(void)
		{
			long ser = 0;
			for (int i = 0; i < rx->data[2] - 1; i++) {
				ser += (long)rx->data[4 + i] << 8 * (3 - i);
			}
			itlDevice->SerailNumber = ser.ToString();

			return true;
		}

		bool ParseIPData(void)
		{
			itlDevice->IPAddress = Encoding::ASCII->GetString(rx->data, 4, rx->data[2] - 1);

			return true;

		}

		bool ParseCurrencyData(int index)
		{
			// clear down for new index
			if (index == 0 && itlDevice->currencies) {
				itlDevice->currencies->Clear();
			}

			ItlCurrency^ cur = gcnew ItlCurrency();

			cur->countryCode = Encoding::ASCII->GetString(rx->data, 6, 3);
			cur->version = Encoding::ASCII->GetString(rx->data, 9, 8);
			itlDevice->currencies->Add(cur);


			return true;
		}

		bool ParsePoll(void)
		{

			for (int i = 1; i < rx->length; i++) {


				switch ((ItlSSPEvents)rx->data[i]) {

				case reset:
					break;
				case billRead:
					if (rx->data[i + 1] > 0) {
						itlDevice->billInEscrow = true;
						itlDevice->escrowBill->countryCode = Encoding::ASCII->GetString(rx->data, i + 1, 3);
						itlDevice->escrowBill->value = 0;
						for (int j = 0; j < 4; j++) {
							itlDevice->escrowBill->value += (int)rx->data[i + 3 + j] << (8 * i);
						}
						i += 7;  // index event for bill data
					}
					else {
						i += 1; // index for 0 channel data
						itlDevice->billInEscrow = false;
					}
					break;
				case disabled:
					
					break;
				case billCredit:
					itlDevice->creditBill->countryCode = Encoding::ASCII->GetString(rx->data, i + 1, 3);
					itlDevice->creditBill->value = 0;
					for (int j = 0; j < 4; j++) {
						itlDevice->creditBill->value += (int)rx->data[i + 4 + j] << (8 * i);
					}
					itlDevice->newBillCredit = true;
					i += 7;  // index event for bill data
					break;
				case billStacking:
					break;
				case billStacked:
					break;
				case billRejecting:
					break;
				case billRejected:
					break;
				case inhibitsSet:
					break;
				case cashboxRemoved:
					break;
				case cashboxReplaced:
					break;
				case safeJam:
					break;
				case unsafeJam:
					break;
				case billStackerFull:
					break;
				case billPathOpen:
					break;
				default:
					break;

				}


			}
			return true;
		}



	};

}
