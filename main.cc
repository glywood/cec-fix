
#include <bcm_host.h>
#include <iostream>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

int serial_fd;
bool want_on = 0;

void serial_power_off() {
	std::cerr << "Turning off the TV" << std::endl;
	write(serial_fd, "ka 00 00\r", 9);
	want_on = false;
}

void cec_callback(void *callback_data, uint32_t reason, uint32_t param1, uint32_t param2, uint32_t param3, uint32_t param4) {
	std::cerr << "Got a callback!" << std::endl << std::hex <<
		"reason = 0x" << reason << std::endl <<
		"param1 = 0x" << param1 << std::endl <<
		"param2 = 0x" << param2 << std::endl <<
		"param3 = 0x" << param3 << std::endl <<
		"param4 = 0x" << param4 << std::endl;

	VC_CEC_MESSAGE_T message;
	if (vc_cec_param2message(reason, param1, param2, param3, param4,
				 &message) == 0) {
		std::cerr << std::hex <<
			"Translated to message i=" << message.initiator <<
			" f=" << message.follower <<
			" len=" << message.length <<
			" content=" << (uint32_t)message.payload[0] <<
			" " << (uint32_t)message.payload[1] <<
			" " << (uint32_t)message.payload[2] << std::endl;

		// Detect when the TV is being told to turn on. Check the power
		// status of the receiver, because if it's not on we'll want to
		// turn it on.
		if (message.length == 1 &&
		    message.payload[0] == CEC_Opcode_ImageViewOn) {
			std::cerr << "ImageViewOn, checking power status of receiver." << std::endl;
			uint8_t bytes[1];
			bytes[0] = CEC_Opcode_GiveDevicePowerStatus;
			if (vc_cec_send_message(CEC_AllDevices_eAudioSystem,
					       	bytes, 1, VC_FALSE) != 0) {
				std::cerr << "Failed to check power status." << std::endl;
			}
			want_on = true;
		}

		if (message.length == 2 &&
		    message.initiator == CEC_AllDevices_eAudioSystem &&
		    message.payload[0] == CEC_Opcode_ReportPowerStatus &&
		    want_on) {
			std::cerr << "Receiver has power status " << (int)message.payload[1] << ". (0=on, 1=off, 2=on_pending, 3=off_pending)" << std::endl;
			if (message.payload[1] == CEC_POWER_STATUS_ON ||
			    message.payload[1] == CEC_POWER_STATUS_ON_PENDING) {
				std::cerr << "Receiver is on now." << std::endl;
				want_on = false;
			} else {
				std::cerr << "Receiver is off but we want it on." << std::endl;
				uint8_t bytes[2];
				bytes[0] = CEC_Opcode_UserControlPressed;
				bytes[1] = CEC_User_Control_Power;
				if (vc_cec_send_message(CEC_AllDevices_eAudioSystem,
							bytes, 2, VC_FALSE) != 0) {
					std::cerr << "Failed to press Power On." << std::endl;
				}
			}
		}

		// As soon as the power-on button press is finished sending,
		// also send a button release.
		if ((reason & VC_CEC_TX) &&
		    message.length == 2 &&
		    message.payload[0] == CEC_Opcode_UserControlPressed &&
		    message.payload[1] == CEC_User_Control_Power) {
			std::cerr << "Power on press complete, now sending release." << std::endl;
			uint8_t bytes[1];
			bytes[0] = CEC_Opcode_UserControlReleased;
			if (vc_cec_send_message(CEC_AllDevices_eAudioSystem,
						bytes, 1, VC_FALSE) != 0) {
				std::cerr << "Failed to release Power On." << std::endl;
			}
		}

		// As soon as the power-on button release is finished sending,
		// query the power status again.
		if ((reason & VC_CEC_TX) &&
		    message.length == 1 &&
		    message.payload[0] == CEC_Opcode_UserControlReleased) {
			std::cerr << "Power on release complete, now querying power status." << std::endl;
			uint8_t bytes[1];
			bytes[0] = CEC_Opcode_GiveDevicePowerStatus;
			if (vc_cec_send_message(CEC_AllDevices_eAudioSystem,
						bytes, 1, VC_FALSE) != 0) {
				std::cerr << "Failed to release Power On." << std::endl;
			}
		}

		// Detect when the TV is being told to go into standby. It
		// ignores that command, so force it to power off using the
		// serial port instead.
		if (message.follower == 0 &&
		    message.length == 1 &&
		    message.payload[0] == CEC_Opcode_Standby) {
			serial_power_off();
		}
	}
}

void tv_callback(void *callback_data, uint32_t reason, uint32_t p0, uint32_t p1) {
	std::cerr << "Got a TV callback!" << std::endl << std::hex <<
		"reason = 0x" << reason << std::endl << 
		"param0 = 0x" << p0 << std::endl <<
		"param1 = 0x" << p1 << std::endl;
}

int main(int argc, char *argv[]) {
	bcm_host_init();
	vcos_init();

	VCHI_INSTANCE_T vchi_instance;
	if (vchi_initialise(&vchi_instance) != 0) {
		std::cerr << "Could not initiaize VHCI" << std::endl;
		return 1;
	}

	if (vchi_connect(nullptr, 0, vchi_instance) != 0) {
		std::cerr << "Failed to connect to VHCI" << std::endl;
		return 1;
	}

	vc_vchi_cec_init(vchi_instance, nullptr, 0);

	if (vc_cec_set_passive(VC_TRUE) != 0) {
		std::cerr << "Failed to enter passive mode" << std::endl;
		return 1;
	}

	vc_cec_register_callback(cec_callback, nullptr);
	vc_tv_register_callback(tv_callback, nullptr);

	if (vc_cec_register_all() != 0) {
		std::cerr << "Failed to register all opcodes" << std::endl;
		return 1;
	}

	vc_cec_register_command(CEC_Opcode_GivePhysicalAddress);
	vc_cec_register_command(CEC_Opcode_GiveDeviceVendorID);
	vc_cec_register_command(CEC_Opcode_GiveOSDName);
	vc_cec_register_command(CEC_Opcode_GetCECVersion);
	vc_cec_register_command(CEC_Opcode_GiveDevicePowerStatus);
	vc_cec_register_command(CEC_Opcode_MenuRequest);
	vc_cec_register_command(CEC_Opcode_GetMenuLanguage);

	if (vc_cec_set_logical_address(CEC_AllDevices_eTV, CEC_DeviceType_TV, CEC_VENDOR_ID_BROADCOM) != 0) {
		std::cerr << "Failed to set logical address" << std::endl;
		return 1;
	}

	serial_fd = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY);
	if (serial_fd < 0) {
		perror("failed to open /dev/ttyUSB0");
		return 1;
	}

	struct termios tio;
	tcgetattr(serial_fd, &tio);
	tio.c_cflag = B9600 | CRTSCTS | CS8 | CLOCAL | CREAD;
	tcsetattr(serial_fd, TCSANOW, &tio);

	std::cerr << "Press CTRL-c to exit" << std::endl;
	std::cerr << "Controls: " << std::endl;
	std::cerr << "  i   Send ImageViewOn to receiver" << std::endl;
	std::cerr << "  s   Broadcast Standby message" << std::endl;
	std::cerr << "  p   Request receiver power status" << std::endl;
	std::cerr << std::endl;
	while (true) {
		char ch;
		std::cin >> ch;

		if (ch == 'i') {
			std::cerr << "ImageViewOn" << std::endl;
			uint8_t bytes[1];
			bytes[0] = CEC_Opcode_ImageViewOn;
			if (vc_cec_send_message(CEC_AllDevices_eAudioSystem,
					       	bytes, 2, VC_FALSE) != 0) {
				std::cerr << "Failed to press Power On." << std::endl;
			}
		} else if (ch == 's') {
			std::cerr << "Broadcast Standby" << std::endl;
			uint8_t bytes[1];
			bytes[0] = CEC_Opcode_Standby;
			if (vc_cec_send_message(CEC_BROADCAST_ADDR,
					       	bytes, 2, VC_FALSE) != 0) {
				std::cerr << "Failed to press Power On." << std::endl;
			}
		} else if (ch == 'p') {
			std::cerr << "GiveDevicePowerStatus" << std::endl;
			uint8_t bytes[1];
			bytes[0] = CEC_Opcode_GiveDevicePowerStatus;
			if (vc_cec_send_message(CEC_AllDevices_eAudioSystem,
					       	bytes, 2, VC_FALSE) != 0) {
				std::cerr << "Failed to press Power On." << std::endl;
			}
		}
	}
	return 0;
}
