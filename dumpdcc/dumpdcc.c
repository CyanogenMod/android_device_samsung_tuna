#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <linux/i2c-dev.h>

#define LOG_TAG "dumpdcc"
#include <cutils/log.h>

#define I2C_DEVICE "/dev/i2c-2"
#define I2C_ADDRESS 0x3E
#define MTD_DEVICE "/dev/mtd/mtd0"
#define DCC_FILE "/data/misc/camera/R5_MVEN003_LD2_ND0_IR0_SH0_FL1_SVEN003_DCCID1044/calib.bin"

int i2c_file;
unsigned char buf[2];
unsigned char *dcc_data;

void i2c_init() {
	if ((i2c_file = open(I2C_DEVICE, O_RDWR)) < 0)
		ALOGE("Failed to open " I2C_DEVICE ": %s", strerror(errno));

	if (ioctl(i2c_file, I2C_SLAVE, I2C_ADDRESS) < 0)
		ALOGE("Failed to set slave address: %s", strerror(errno));
}

void i2c_seq_1() {
	int ret;

	buf[0] = 0x08;
	buf[1] = 0x0A;
	ret = write(i2c_file, buf, 2);
	if (ret < 0)
		ALOGE("Failed writing to i2c: %s", strerror(errno));

	buf[0] = 0x00;
	ret = write(i2c_file, buf, 1);
	if (ret < 0)
		ALOGE("Failed writing to i2c: %s", strerror(errno));

	ret = read(i2c_file, buf, 1);
	if (ret < 0)
		ALOGE("Failed reading from i2c: %s", strerror(errno));

	buf[0] = 0x00;
	buf[1] = 0x05;
	ret = write(i2c_file, buf, 2);
	if (ret < 0)
		ALOGE("Failed writing to i2c: %s", strerror(errno));
}

void i2c_seq_2() {
	int ret;

	buf[0] = 0x08;
	buf[1] = 0x0A;
	ret = write(i2c_file, buf, 2);
	if (ret < 0)
		ALOGE("Failed writing to i2c: %s", strerror(errno));

	buf[0] = 0x00;
	ret = write(i2c_file, buf, 1);
	if (ret < 0)
		ALOGE("Failed writing to i2c: %s", strerror(errno));

	ret = read(i2c_file, buf, 1);
	if (ret < 0)
		ALOGE("Failed reading from i2c: %s", strerror(errno));

	buf[0] = 0x00;
	buf[1] = 0x01;
	ret = write(i2c_file, buf, 2);
	if (ret < 0)
		ALOGE("Failed writing to i2c: %s", strerror(errno));
}

/*
 * Validate the extracted DCC data
 */
int validate_dcc() {
	if (dcc_data[0x00] == 0x4F &&
		dcc_data[0x01] == 0x41 &&
		dcc_data[0x02] == 0x45 &&
		dcc_data[0x03] == 0x4A) {
		return 0;
	} else return 1;
}

/*
 * Extract DCC data from flash memory
 */
int extract_dcc() {
	int ret;
	i2c_init();
	i2c_seq_1();

	dcc_data = (unsigned char*) malloc(0x225);

	int mtd_file = open(MTD_DEVICE, O_RDONLY);
	if (mtd_file < 0) {
		ALOGE("Failed to open " MTD_DEVICE ": %s", strerror(errno));
		ret = errno;
		goto exit;
	}

	ret = read(mtd_file, dcc_data, 0x225);
	if (ret < 0) {
		ALOGE("Error reading from " MTD_DEVICE ": %s", strerror(errno));
		ret = errno;
	}

	close(mtd_file);

exit:
	i2c_seq_2();
	close(i2c_file);
	return ret;
}

/*
 * Write DCC file
 */
int write_dcc() {
	int ret;
	unsigned char dcc_header[0x54] = {0};
	dcc_header[0x00] = 0x14;
	dcc_header[0x01] = 0x04;
	dcc_header[0x04] = 0x64;
	dcc_header[0x08] = 0x01;
	dcc_header[0x14] = 0xCF;
	dcc_header[0x15] = 0xCA;
	dcc_header[0x16] = 0x1C;
	dcc_header[0x17] = 0x2C;
	dcc_header[0x18] = 0x6D;
	dcc_header[0x40] = 0x25;
	dcc_header[0x41] = 0x02;
	dcc_header[0x4C] = 0x79;
	dcc_header[0x4D] = 0x02;

	int dcc_file = open(DCC_FILE, O_CREAT | O_TRUNC | O_WRONLY, S_IRWXU);
	if (dcc_file < 0) {
		ALOGE("Failed to open " DCC_FILE ": %s", strerror(errno));
		return errno;
	}

	ret = write(dcc_file, dcc_header, 0x54);
	if (ret < 0) {
		ALOGE("Failed writing DCC header to " DCC_FILE ": %s", strerror(errno));
		return errno;
	}

	ret = write(dcc_file, dcc_data, 0x225);
	if (ret < 0) {
		ALOGE("Failed writing DCC data to " DCC_FILE ": %s", strerror(errno));
		return errno;
	}

	close(dcc_file);
	free(dcc_data);

	return ret;
}

int main() {
	if (extract_dcc() < 0) {
		ALOGE("Failed to read DCC data, aborting");
		return 1;
	}

	if (validate_dcc() != 0) {
		ALOGE("Failed to validate DCC data, aborting");
		return 1;
	}

	if (write_dcc() < 0) {
		ALOGE("Failed to write DCC data");
		return 1;
	}

	ALOGI("DCC data successfully saved");
	return 0;
}
