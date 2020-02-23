
#include <bcm_host.h>
#include <iostream>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

int serial_fd;

void serial_power_off() {
	std::cerr << "Turning off the TV" << std::endl;
	write(serial_fd, "ka 00 00\r", 9);
}

void cec_callback(void *callback_data, uint32_t reason, uint32_t param1, uint32_t param2, uint32_t param3, uint32_t param4) {
	std::cerr << "Got a callback!" << std::endl << std::hex <<
		"reason = 0x" << reason << std::endl <<
		"param1 = 0x" << param1 << std::endl <<
		"param2 = 0x" << param2 << std::endl <<
		"param3 = 0x" << param3 << std::endl <<
		"param4 = 0x" << param4 << std::endl;

	VC_CEC_MESSAGE_T message;
	if (vc_cec_param2message(reason, param1, param2, param3, param4, &message) == 0) {
		std::cerr << std::hex << "Translated to message i=" << message.initiator << " f=" << message.follower << " len=" << message.length << " content=" << (uint32_t)message.payload[0] << " " << (uint32_t)message.payload[1] << " " << (uint32_t)message.payload[2] << std::endl;
		if (message.initiator == CEC_AllDevices_eAudioSystem && message.length == 2 && message.payload[0] == 0x72 && message.payload[1] == 0x00) {
			std::cerr << "Set system audio mode command detected. Pressing power on." << std::endl;
			uint8_t bytes[2];
			bytes[0] = CEC_Opcode_UserControlPressed;
			bytes[1] = CEC_User_Control_Power;
			if (vc_cec_send_message(CEC_AllDevices_eAudioSystem, bytes, 2, VC_FALSE) != 0) {
				std::cerr << "Failed to press Power On." << std::endl;
			}
		}

		if ((reason & VC_CEC_TX) && message.length == 2 && message.payload[0] == 0x44 && message.payload[1] == 40) {
			std::cerr << "Power on press complete, now sending release" << std::endl;
			uint8_t bytes[1];
			bytes[0] = CEC_Opcode_UserControlReleased;
			if (vc_cec_send_message(CEC_AllDevices_eAudioSystem, bytes, 1, VC_FALSE) != 0) {
				std::cerr << "Failed to release Power On." << std::endl;
			}
		}

		if (message.follower == 0 && message.length == 1 && message.payload[0] == CEC_Opcode_Standby) {
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
	while (true) {
		sleep(1000);
	}
	return 0;
}
