//*----------------------------------------------------------------------------
//*         Innovative Technology Ltd  - T.Beswick   - March 2017
//*----------------------------------------------------------------------------
//* The software is delivered "AS IS" without warranty or condition of any
//* kind, either express, implied or statutory. This includes without
//* limitation any warranty or condition with respect to merchantability or
//* fitness for any particular purpose, or against the infringements of
//* intellectual property rights of others.
//*----------------------------------------------------------------------------
//* File Name           : smart-curr-clr.cpp
//* Description			: A command line program for ITL Smart Currency
//*----------------------------------------------------------------------------

#include "stdafx.h"

#include "ItlSystemPort.h"
#include "ItlSSP.h"
using namespace System;
using namespace ItlSSPSystem;
using namespace System::Threading;
using namespace System::IO;


/*enum for escrow fucntions*/
typedef enum {
	hold,
	acceptBill,
	rejectBill
}EscrowAction;


/*
* Main class
*/
public ref class SmartCurrency
{

private:

	static String^ port = "";
	static ItlSystemPort^ sys;
	static ItlSSP^ ssp;
	static bool _continue;
	static EscrowAction escrowAction = hold;
	static bool useEscrow = false;
	static bool indexDataSets = false;
	static bool newCountryCheck = false;
	static String^ countryToCheck;
	static int newInhibitValue = -1;
	static int newBillInhibitValue = -1;
	static bool newInhibitSet = false;
	static bool newDenominationInhibitSet = false;
	static bool newCurrencyFileDelete = false;
	static bool newUploadFile = false;
	static array<unsigned char>^ uploadData;
	static SystemState stateAfterDisable;


public:

	/*
	* Menu and help functions
	*/

	static bool ParseArgs(array<String^>^ args)
	{

		bool portSet = false;

		for each (String^ arg in args)
		{
			
			String^ sb = arg->Substring(0, 2);

			if (String::Compare(sb, "-h") == 0) {
				DisplayHelp();
				return false;
			}


			if (String::Compare(sb, "-e") == 0) {
				useEscrow = true;					 
			}
			if (String::Compare(sb, "-i") == 0) {
				indexDataSets = true;
			}

			if (String::Compare(sb, "-p") == 0) {

				port = arg->Substring(2, arg->Length - 2);
				portSet = true;
			}

		}
		
		if (!portSet) {
			Console::WriteLine("Required port parameter -p not set. See below:");
			DisplayHelp();
			return false;
		}

		return true;
	}

	static void DisplayHelp(void)
	{
		Console::WriteLine("Smart Currency command line utility");

		Console::WriteLine("-p[port id] ex: -pCOM4" );
		Console::WriteLine("-e Optional: Set to use escrow mode. Default: do not use escrow");
		Console::WriteLine("-i Optional: Set to index datasets at start-up. Default: do not index");

		Console::WriteLine("");
		Console::WriteLine("Menu keys during run:");
		Console::WriteLine("'C' check a supported country code. e.g. GBP");
		Console::WriteLine("'L' display full list of loaded country parameters (only if currecyIndex option set)");
		Console::WriteLine("'I' Set a currency global inhibit (0 = not inhibited, 1 = inhibited)");
		Console::WriteLine("'D' Set a currency denomination inhibit (0 = not inhibited, 1 = inhibited)");
		Console::WriteLine("'X' delete a currency file");
		Console::WriteLine("'U' upload a file for update");

		Console::WriteLine(" ");
		Console::WriteLine("Press return key to close...");

		Console::Read();

	}


	/*
	* The main loop
	*/
	static void Main(array<System::String ^> ^args)
	{

		if (!ParseArgs(args)) {
			return;
		}



		// data array for commands
		int curIndex = 0;
		int waitTime = 20;
		int ind,spd,timeout;
		array<unsigned char>^ data = gcnew array<unsigned char>(128);

		// thread to handle user input
		Thread^ td = gcnew Thread(gcnew ThreadStart(SmartCurrency::GetUserInput));
		td->Start();

		// SSP objects
		sys = gcnew ItlSystemPort();
		ssp = gcnew ItlSSP(sys);
		ssp->itlDevice->useEscrow = useEscrow;
		// open serial port unsing passed port parameter
		if (!ssp->OpenPort(port)) {
			Console::WriteLine("Unable to open com port: " + port);			
		}
		else {
			_continue = true;
			ssp->state = connect;
			array<unsigned char>^ dt; 
			// main State machine loop
			while (_continue) {
				Thread::Sleep(waitTime); 

				// function flags
				if (newCountryCheck) {
					Console::WriteLine("Checking country code: " + countryToCheck);
					newCountryCheck = false;
					ssp->state = checkCode;
				}
				if (newInhibitSet) {
					Console::WriteLine("Setting inhibit value for " + countryToCheck);
					ssp->state = setInhibit;
					newInhibitSet = false;
				}
				if (newDenominationInhibitSet) {
					Console::WriteLine("Setting inhibit for " + countryToCheck + " " + newBillInhibitValue.ToString());
					ssp->state = setDenominationInhibit;
					newDenominationInhibitSet = false;
				}
				if (newCurrencyFileDelete) {
					ssp->state = setFileDelete;
					newCurrencyFileDelete = false;
				}
				if (newUploadFile) {
					ssp->state = setDisable;
					stateAfterDisable = setNewFileUpload;
					newUploadFile = false;
				}

				// state machine functions
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
					if (ssp->SSPCommand(cmdSetUpRequest, nullptr, 0) && ssp->genResponseOK) {
						if (ssp->ParseSetUpData()) {
							Console::WriteLine(" ");
							Console::WriteLine("Smart Currency device found");
							Console::WriteLine("Loaded with " + ssp->itlDevice->numberOfCurrencies.ToString() + " currency files");
						}
						else {
							Console::WriteLine("Setup request command failed");
							_continue = false;
							break;
						}
					}
					else {
						// if the device is busy, retry after a delay
						if (ssp->busyResponse) {
							Console::Write(".");
							Thread::Sleep(200); // give it a chance
							break;
						}
						else {
							Console::WriteLine("Setup request command failed");
							ssp->state = connect;
							break;
						}

					}	
					if (ssp->SSPCommand(cmdGetFirmware, nullptr, 0) && ssp->ParseFirmwareData()) {
						Console::WriteLine("Firmware version: " + ssp->itlDevice->FirmwareVersion);
					}
					else {
						Console::WriteLine("Firmware request command failed");
						ssp->state = connect;
						break;
					}
					if (ssp->SSPCommand(cmdGetSerial, nullptr, 0) && ssp->ParseSerialNumberData()) {
						Console::WriteLine("Firmware version: " + ssp->itlDevice->SerailNumber);
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

					if (indexDataSets && ssp->itlDevice->numberOfCurrencies) {
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
							Console::WriteLine("New bill escrow: " + ssp->itlDevice->escrowBill->countryCode + " " + ssp->itlDevice->escrowBill->value);
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
				case checkCode:
					dt = Encoding::ASCII->GetBytes(countryToCheck);
					if (ssp->SSPCommand(cmdCountrySupported, dt, 3) && ssp->ParseSupportedCountry()) {
						Console::WriteLine("Code is supported:");
						Console::WriteLine(ssp->itlDevice->supportedCountry->countryCode);
						Console::WriteLine(ssp->itlDevice->supportedCountry->version);
						Console::WriteLine("CRC OK: " + ssp->itlDevice->supportedCountry->crcStatus.ToString());
						Console::WriteLine("Currency inhibited: "  + (ssp->itlDevice->supportedCountry->inhibitStatus == 0 ? "no": "yes"));
					}
					else {
						Console::WriteLine(countryToCheck + " code is not supported!");
					}
					ssp->state = run;
					break;
				case setInhibit:
					dt= Encoding::ASCII->GetBytes(countryToCheck);
					data[0] = dt[0]; data[1] = dt[1];  data[2] = dt[2];
					data[3] = newInhibitValue;
					if (ssp->SSPCommand(cmdSetCountryInhibit, data, 4) && ssp->genResponseOK) {
						Console::WriteLine(countryToCheck + " inhibit changed");
					}
					else {
						Console::WriteLine(countryToCheck + " code is not supported in this device");
					}
					ssp->state = run;
					break;
				case setDenominationInhibit:
					dt = Encoding::ASCII->GetBytes(countryToCheck);
					data[0] = dt[0]; data[1] = dt[1];  data[2] = dt[2];
					for (int i = 0; i < 4; i++) {
						data[i + 3] = (unsigned char)(newBillInhibitValue >> (8 * i));
					}
					data[7] = newInhibitValue;
					if (ssp->SSPCommand(cmdSetDenominationInhibit, data, 8) && ssp->genResponseOK) {
						Console::WriteLine(countryToCheck + " " + newBillInhibitValue.ToString() +  " inhibit changed");
					}
					else {
						Console::WriteLine(countryToCheck + " code is not supported in this device");
					}
					ssp->state = run;
					break;	
				case setDisable:
					if (ssp->SSPCommand(cmdDisable, nullptr, 0)) {
						// make sure the post disable state has been set
						ssp->state = stateAfterDisable;
					}
					else {
						Console::WriteLine("Unable to disable device. Restarting...");
						ssp->state = connect;
					}
					break;
				case setFileDelete:
					dt = Encoding::ASCII->GetBytes(countryToCheck);
					data[0] = 0x00; // file delete sub command
					for (int i = 0; i < dt->Length ; i++) {
						data[1 + i] = dt[i];
					}
					if (ssp->SSPCommand(cmdFileOperations, data, 9) && ssp->genResponseOK) {
						Console::WriteLine(countryToCheck + " currency file deleted from system");
					}
					else {
						Console::WriteLine(countryToCheck + " code is not supported in this device");
					}
					ssp->state = run;
					break;
				case setNewFileUpload:
					dt = Encoding::ASCII->GetBytes(countryToCheck);
					data[0] = 0x01; // file upload sub command
					data[1] = 0x00; //type upload
					data[2] = dt->Length; // the length of the file name
					ind = 3;
					for (int i = 0; i < dt->Length; i++) {
						data[ind++] = dt[i];
					}
					for (int i = 0; i < 4; i++) { // the file data length
						data[ind++] = (unsigned char)(uploadData->Length >> (8 * i));
					}
					// the file crc
					for (int i = 0; i < 2; i++) {
						data[ind++] = 0;
					}
					// upload speed
					spd = 115200;
					for (int i = 0; i < 4; i++) {
						data[ind++] = (unsigned char)(spd >> (8 * i));
					}
					// upload timeout (ms)
					timeout = 500;
					for (int i = 0; i < 4; i++) {
						data[ind++] = (unsigned char)(timeout >> (8 * i));
					}
					if (ssp->SSPCommand(cmdFileOperations, data, ind) && ssp->genResponseOK) {
						Console::WriteLine(countryToCheck + "  file ready for upload to system");
						ssp->state = sendUploadFileData;
					}
					else {
						Console::WriteLine(countryToCheck + " error setting file upload");
						ssp->state = run;
					}
					break;
				case sendUploadFileData:
					Console::WriteLine("Sending upload file data. Please wait...");
					if (ssp->WriteBulkData(uploadData, uploadData->Length)) {
						Console::WriteLine(countryToCheck + " file uploaded OK");
					}
					else {
						Console::WriteLine(countryToCheck + " file upload failed!");
					}
					ssp->state = enable;
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


			// only menu keys on running  status
			if (ssp->state == run) {

				if (indexDataSets && cki->Key == ConsoleKey::L) {
					Console::WriteLine("List of stored datasets:");
					int index = 0;
					for each (ItlCurrency^ cur in ssp->itlDevice->currencies)
					{
						Console::WriteLine("Index " + index.ToString());
						index++;
						Console::WriteLine(cur->countryCode);
						Console::WriteLine(cur->version);
						Console::WriteLine("CRC OK: " + cur->crcStatus.ToString());
						Console::WriteLine("Currency inhibited: " + (cur->inhibitStatus == 0 ? "no" : "yes"));
						Console::WriteLine("-------------------------------------------------------------");
					}
				}
				if (cki->Key == ConsoleKey::C) {
					Console::WriteLine("enter the country code to check: ");
					String^ ds = Console::ReadLine();
					countryToCheck = ds->ToUpper();
					if (ds->Length != 3) {
						Console::WriteLine("Invalid country code given. Example: GBP");
					}
					else {
						newCountryCheck = true;
					}

				}
				if (cki->Key == ConsoleKey::I) {
					Console::WriteLine("enter the country code to inhibit: ");
					String^ ds = Console::ReadLine();
					countryToCheck = ds->ToUpper();
					if (ds->Length != 3) {
						Console::WriteLine("Invalid country code given. Example: GBP");
					}
					else {
						Console::WriteLine("Enter new inhibit value. 0 for not inhibit, 1 for inhibit:");
						String^ inh = Console::ReadLine();
						newInhibitValue = Convert::ToInt32(inh);
						if (newInhibitValue != 0 && newInhibitValue != 1) {
							Console::WriteLine("Inhibit value must be 0 or 1");
							newInhibitValue = -1;							
						}else{
							newInhibitSet = true;
						}
						
					}

				}
				if (cki->Key == ConsoleKey::D) {
					Console::WriteLine("enter the country code to inhibit: ");
					String^ ds = Console::ReadLine();
					countryToCheck = ds->ToUpper();
					if (ds->Length != 3) {
						Console::WriteLine("Invalid country code given. Example: GBP");
					}
					else {

						Console::WriteLine("Enter the bill value:");
						String^ inb = Console::ReadLine();
						newBillInhibitValue = Convert::ToInt32(inb);
						Console::WriteLine("Enter new inhibit value. 0 for not inhibit, 1 for inhibit:");
						String^ inh = Console::ReadLine();
						newInhibitValue = Convert::ToInt32(inh);
						if (newInhibitValue != 0 && newInhibitValue != 1) {
							Console::WriteLine("Inhibit value must be 0 or 1");
							newInhibitValue = -1;
						}
						else {
							newDenominationInhibitSet = true;
						}

					}
				}
				if (cki->Key == ConsoleKey::X) {
					Console::WriteLine("Enter the full currency version to delete (e.g. GBP01S23)");
					String^ ds = Console::ReadLine();
					countryToCheck = ds->ToUpper();
					if (ds->Length != 8) {
						Console::WriteLine("Invalid version code given. Example: GBP11S01");
					}
					else {
						newCurrencyFileDelete = true;
					}
				}
				if (cki->Key == ConsoleKey::U) {
					Console::WriteLine("Enter file path location to upload:");
					String^ ds = Console::ReadLine();
					countryToCheck = ds;
					// load the file data
					try {
						FileStream^ fs = gcnew FileStream(ds, FileMode::Open);
						BinaryReader^ br = gcnew BinaryReader(fs);
						uploadData = br->ReadBytes(fs->Length);
						newUploadFile = true;
						br->Close();
						fs->Close();
					}
					catch (Exception^ ex) {
						Console::WriteLine("Unable to open file for upload " + ex->Message);
						newUploadFile = false;
						
					}

					
					
				}



			}
		}

	}


};

/* entry point */
int main(array<System::String ^> ^args)
{
	SmartCurrency::Main(args);

}









