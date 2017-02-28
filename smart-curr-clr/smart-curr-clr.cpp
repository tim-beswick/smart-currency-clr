// smart-curr-clr.cpp : main project file.

#include "stdafx.h"

#include "ItlSystemPort.h"
#include "ItlSSP.h"
using namespace System;
using namespace ItlSSPSystem;
using namespace System::Threading;



typedef enum {
	hold,
	acceptBill,
	rejectBill
}EscrowAction;


public ref class SmartCurrency
{

private:

	static String^ port = "COM4";
	static ItlSystemPort^ sys;
	static ItlSSP^ ssp;
	static Boolean _continue;
	static EscrowAction escrowAction = hold;


public:




	static void Main()
	{

		// data array for commands
		int curIndex = 0;
		int waitTime = 20;
		array<unsigned char>^ data = gcnew array<unsigned char>(32);

		// thread to handle user input
		Thread^ td = gcnew Thread(gcnew ThreadStart(SmartCurrency::GetUserInput));
		td->Start();

		sys = gcnew ItlSystemPort();
		ssp = gcnew ItlSSP(sys);
		if (!ssp->OpenPort(port)) {
			Console::WriteLine("Unable to open com port: " + port);			
		}
		else {
			_continue = true;
			ssp->state = connect;
			while (_continue) {
				Thread::Sleep(waitTime); 

				switch (ssp->state) {
				case connect:
					waitTime = 20;
					// send sync command
					if (!ssp->SSPCommand(cmdSync, nullptr, 0)) {
						Console::WriteLine("SYNC command failed");
					}
					else {
						Console::WriteLine("SYNC connection found - initialising data...");
						ssp->state = initialise;
					}
					break;
				case initialise:
					if (ssp->SSPCommand(cmdSetUpRequest, nullptr, 0) && ssp->ParseSetUpData()) {
						Console::WriteLine("Smart Currency device found");
						Console::WriteLine("Loaded with " + ssp->itlDevice->numberOfCurrencies.ToString() + " currency files");
					}
					else {
						Console::WriteLine("Setup request command failed");
						ssp->state = connect;
						break;
					}	
					if (ssp->SSPCommand(cmdGetFirmware, nullptr, 0) && ssp->ParseFirmwareData()) {

					}
					else {
						Console::WriteLine("Firmware request command failed");
						ssp->state = connect;
						break;
					}
					if (ssp->SSPCommand(cmdGetSerial, nullptr, 0) && ssp->ParseSerialNumberData()) {
						Console::WriteLine("Firmware version: " + ssp->itlDevice->FirmwareVersion);
					}
					else {
						Console::WriteLine("Serial number command failed");
						ssp->state = connect;
						break;
					}
					if (ssp->SSPCommand(cmdGetIP, nullptr, 0) && ssp->ParseIPData()) {
						Console::WriteLine("IP address: " + ssp->itlDevice->IPAddress);
					}
					else {
						Console::WriteLine("IP request command failed");
						ssp->state = connect;
						break;
					}


					ssp->itlDevice->numberOfCurrencies = 0;




					if (ssp->itlDevice->numberOfCurrencies) {
						Console::WriteLine("Indexing currencies...");
						ssp->state = indexCurrencies;
						curIndex = 0;
					}
					else {
						ssp->state = enable;
					}
					break;
				case indexCurrencies:

					data[0] = (unsigned char)curIndex;
					if (ssp->SSPCommand(cmdCountryData, data, 1) && ssp->ParseCurrencyData(curIndex)) {
						//Console::Write(ssp->itlDevice->currencies[curIndex]->countryCode);
						//Console::Write(" ");
						//Console::WriteLine(ssp->itlDevice->currencies[curIndex]->version);
						Console::Write(".");
					}
					else {
						Console::WriteLine("Currency command failed at index " + curIndex.ToString());
						ssp->state = connect;
						break;
					}
					curIndex++;
					if (curIndex >= ssp->itlDevice->numberOfCurrencies)
					{
						Console::WriteLine("");
						Console::WriteLine("Index complete");
						ssp->state = enable;
					}
					break;
				case enable:
					if (ssp->SSPCommand(cmdEnable, nullptr, 0)) {
						Console::WriteLine("Device enable");
						waitTime = 200;
						ssp->state = run;
					}
					else {
						Console::WriteLine("enable command failed at index " + curIndex.ToString());
						ssp->state = connect;
						break;
					}
					break;
				case run:
					if (ssp->SSPCommand(cmdPoll, nullptr, 0) && ssp->ParsePoll()) {

						if (ssp->itlDevice->billInEscrow) {
							Console::WriteLine("New bill escrow: " + ssp->itlDevice->creditBill->countryCode + " " + ssp->itlDevice->creditBill->value);
							Console::WriteLine("Press 'A' to accept or 'R' to reject...");
							escrowAction = hold;
							ssp->state = holdBill;
						}
						else {

							if (ssp->itlDevice->newBillCredit) {
								ssp->itlDevice->newBillCredit = false;
								Console::WriteLine("New bill credit: " + ssp->itlDevice->creditBill->countryCode + " " + ssp->itlDevice->creditBill->value);
							}

						}

					}
					else {
						Console::WriteLine("Poll command command failed");
						ssp->state = connect;
						break;
					}
					break;
				case holdBill:
					switch (escrowAction) {
					case hold:
						if (!ssp->SSPCommand(cmdHoldBill, nullptr, 0)) {
							ssp->itlDevice->billInEscrow = false;
							ssp->state = run;
						}
						break;
					case acceptBill:
						ssp->itlDevice->billInEscrow = false;
						ssp->state = run;						
						break;
					case rejectBill:
						ssp->SSPCommand(cmdRejectBill, nullptr, 0);
						ssp->state = run;
						break;
					}

					break;

				}




			}

		}

		sys->ClosePort();


	}


	~SmartCurrency() {
		_continue = false;
		delete ssp;
		delete sys;
	}


	static void GetUserInput(void)
	{

		ConsoleKeyInfo^ cki = gcnew ConsoleKeyInfo();

		// wait for loop to start
		while (!_continue)
		{
			Thread::Sleep(100);
		}

		while (_continue) {
			Thread::Sleep(50);
			cki = Console::ReadKey(true);
			if (escrowAction == hold) {
				if (cki->Key == ConsoleKey::A) {
					escrowAction = acceptBill;
				}
				if (cki->Key == ConsoleKey::R) {
					escrowAction = rejectBill;
				}
			}
		}

	}


};


int main(array<System::String ^> ^args)
{

	SmartCurrency::Main();

}









