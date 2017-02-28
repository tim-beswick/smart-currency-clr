#pragma once
#using <System.dll>

using namespace System::IO::Ports;
using namespace System;




	public ref class ItlSystemPort
	{

	private:
		static SerialPort^ _serialPort;
		array<unsigned char>^ readBuffer;

	public:

		ItlSystemPort()
		{
			readBuffer = gcnew array<unsigned char>(255);
		}


		bool OpenPort(String^ port)
		{


			try {
				_serialPort = gcnew SerialPort();
				_serialPort->PortName = port;
				_serialPort->BaudRate = 9600;
				_serialPort->StopBits = StopBits::Two;
				_serialPort->DataBits = 8;
				_serialPort->Handshake = Handshake::None;
				_serialPort->Open();

				

				return true;
			}
			catch (Exception^ ex) {

				Console::WriteLine("Serial port exception: " + ex->Message);

				return false;
			}

		}


		bool WriteData(array<unsigned char>^ data, int length)
		{

			_serialPort->Write(data, 0, length);

			return true;

		}

		array<unsigned char>^ GetBuffer(void)
		{
			return readBuffer;
			
		}


		void ClosePort()
		{
			if (_serialPort->IsOpen) {
				_serialPort->Close();
			}

		}


		bool PortIsOpen(void)
		{
			return _serialPort->IsOpen;
		}


		int Read(void)
		{

			return _serialPort->Read(readBuffer, 0, 255);

		}


	};


