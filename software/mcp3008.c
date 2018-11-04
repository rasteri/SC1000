#include <stdint.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "mcp3008read.h"

#define RETURN_ERR_CODE(code) {mcp3008errno = code; return code;}

#define SPI_DEFAULT_SPEED	MCP3008_SPEED_5V_MAX_HZ
#define SPI_DEFAULT_LEN		MCP3008_LEN
#define SPI_DEFAULT_DELAY	MCP3008_DELAY
#define SPI_DEFAULT_MODE	MCP3008_MODE
#define SPI_DEFAULT_BPW 	MCP3008_BPW


#define	MCP3008_TX_WORD1     0x01	/* 0b00000001 */
#define	MCP3008_TX_WORD2_CH0 0x80	/* 0b10000000 */
#define	MCP3008_TX_WORD2_CH1 0x90	/* 0b10010000 */
#define	MCP3008_TX_WORD2_CH2 0xA0	/* 0b10100000 */
#define	MCP3008_TX_WORD2_CH3 0xB0	/* 0b10110000 */
#define	MCP3008_TX_WORD2_CH4 0xC0	/* 0b11000000 */
#define	MCP3008_TX_WORD2_CH5 0xD0	/* 0b11010000 */
#define	MCP3008_TX_WORD2_CH6 0xE0	/* 0b11100000 */
#define	MCP3008_TX_WORD2_CH7 0xF0	/* 0b11110000 */
#define	MCP3008_TX_WORD3     0x00	/* 0b00000000 */

#define MCP3008_RX_WORD1_MASK 0x00	/* 0b00000000 */
#define MCP3008_RX_WORD2_NULL_BIT_MASK 0x04	/* 0b00000100 */
#define MCP3008_RX_WORD2_MASK 0x03	/* 0b00000011 */
#define MCP3008_RX_WORD3_MASK 0xFF	/* 0b11111111 */


#define	MCP3008_READ_NORM_EXIT_MSG "MCP3008read function exited normally "
#define MCP3008_READ_FAIL_NO_RX_NULL_BIT_MSG "MCP3008read missing null bit "
#define MCP3008_READ_FAIL_OPEN_DEV_MSG "MCP3008read could not open SPI device "
#define MCP3008_READ_FAIL_MISS_PARAM_MSG "MCP3008read missing function parameter "
#define MCP3008_READ_FAIL_IOCTL_MSG "MCP3008read ioctl failed "
#define MCP3008_READ_FAIL_RX_ERR_MSG "MCP3008read receive error "
#define MCP3008_READ_FAIL_INTERNAL_ERR_MSG "MCP3008 internal error "


static uint8_t  spiMode = SPI_DEFAULT_MODE;
static uint8_t  spiBPW = SPI_DEFAULT_BPW;
static uint16_t spiDelay = SPI_DEFAULT_DELAY;
static char     device[20];

static uint8_t  get_tx_word2(int ch);
static void     getmcp3008errormsg(char *tmpstr);

extern int      errno;

/*
 * holds error codes 
 */
int             mcp3008errno;


int
mcp3008read_attr_init(mcp3008read_attr_t * attr, char *spi_dev, uint32_t speed,
		      int num_ch, ...)
{
	va_list         ap;
	int             i = 0;
	int             channel;

	mcp3008errno = 0;

	if (attr == NULL) {
		RETURN_ERR_CODE(MCP3008_READ_FAIL_MISS_PARAM);
	}
	if (spi_dev == NULL) {
		RETURN_ERR_CODE(MCP3008_READ_FAIL_MISS_PARAM);
	} else {
		strcpy(device, spi_dev);
		if ((attr->fd = open(spi_dev, O_RDWR)) < 0) {
			RETURN_ERR_CODE(MCP3008_READ_FAIL_OPEN_DEV);
		}
	}

	if (speed <= 0) {
		attr->spi_speed = SPI_DEFAULT_SPEED;
	} else {
		attr->spi_speed = speed;
	}
	va_start(ap, num_ch);
	for (i = 0; i < num_ch; i++) {
		channel = va_arg(ap, int);
		attr->ch_flags[channel] = true;
		attr->spi_ch_transfer.len = SPI_DEFAULT_LEN;
		attr->spi_ch_transfer.delay_usecs = spiDelay;
		attr->spi_ch_transfer.speed_hz = attr->spi_speed;
		attr->spi_ch_transfer.bits_per_word = SPI_DEFAULT_BPW;
	}
	va_end(ap);

	if (ioctl(attr->fd, SPI_IOC_WR_MODE, &spiMode) < 0)
		RETURN_ERR_CODE(MCP3008_READ_FAIL_IOCTL);
	if (ioctl(attr->fd, SPI_IOC_RD_MODE, &spiMode) < 0)
		RETURN_ERR_CODE(MCP3008_READ_FAIL_IOCTL);

	if (ioctl(attr->fd, SPI_IOC_WR_BITS_PER_WORD, &spiBPW) < 0)
		RETURN_ERR_CODE(MCP3008_READ_FAIL_IOCTL);
	if (ioctl(attr->fd, SPI_IOC_RD_BITS_PER_WORD, &spiBPW) < 0)
		RETURN_ERR_CODE(MCP3008_READ_FAIL_IOCTL);

	if (ioctl(attr->fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0)
		RETURN_ERR_CODE(MCP3008_READ_FAIL_IOCTL);
	if (ioctl(attr->fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed) < 0)
		RETURN_ERR_CODE(MCP3008_READ_FAIL_IOCTL);

	return MCP3008_READ_NORM_EXIT;
}


int
mcp3008read_attr_destroy(mcp3008read_attr_t * attr)
{
	int             i;
	close(attr->fd);
	attr->spi_speed = 0;
	for (i = 0; i < MCP3008_CH_AMOUNT; i++) {
		attr->ch_flags[i] = false;
	}
	return MCP3008_READ_NORM_EXIT;
}


int
mcp3008read(mcp3008read_attr_t * attr, uint10_t * data)
{
	int             i;
	uint8_t         buffer[3];
	uint8_t         rx_word1,
	                rx_word2,
	                rx_word2_nb,
	                rx_word3;
	uint10_t        received;

	for (i = 0; i < MCP3008_CH_AMOUNT; i++) {
		if (attr->ch_flags[i]) {
			/*
			 * TX setup 
			 */
			buffer[0] = MCP3008_TX_WORD1;
			buffer[1] = get_tx_word2(i);
			buffer[2] = MCP3008_TX_WORD3;
			attr->spi_ch_transfer.tx_buf = (unsigned long) buffer;
			attr->spi_ch_transfer.rx_buf = (unsigned long) buffer;
			/*
			 * send request 
			 */
			if (ioctl
			    (attr->fd, SPI_IOC_MESSAGE(1),
			     &attr->spi_ch_transfer) < 0)
				RETURN_ERR_CODE(MCP3008_READ_FAIL_IOCTL);
			/*
			 * validating ... 
			 */
			rx_word1 = buffer[0] & MCP3008_RX_WORD1_MASK;
			if (rx_word1 != 0)
				RETURN_ERR_CODE(MCP3008_READ_FAIL_RX_ERR);

			/*
			 * ... null bit 
			 */
			rx_word2_nb =
			    buffer[1] & MCP3008_RX_WORD2_NULL_BIT_MASK;
			if (rx_word2_nb != 0)
				RETURN_ERR_CODE
				    (MCP3008_READ_FAIL_NO_RX_NULL_BIT);

			rx_word2 = buffer[1] & MCP3008_RX_WORD2_MASK;
			rx_word3 = buffer[2] & MCP3008_RX_WORD3_MASK;

			/*
			 * ... received data 
			 */
			received =
			    ((rx_word2 << 8) | (rx_word3)) &
			    UINT10_VALIDATION_MASK;
			if (received > UINT10_MAX)
				RETURN_ERR_CODE(MCP3008_READ_FAIL_RX_ERR);
			/*
			 * ... validating done 
			 */

			/*
			 * final store 
			 */
			data[i] = received;
		}
	}
	return MCP3008_READ_NORM_EXIT;
}

void
pmcp3008error(char *s)
{
	char           *tmpstr = (char *) calloc(100, sizeof(char));
	getmcp3008errormsg(tmpstr);
	fprintf(stderr, "%s: %s\n", s, tmpstr);
	free(tmpstr);
}

static void
getmcp3008errormsg(char *tmpstr)
{
	switch (mcp3008errno) {
	case MCP3008_READ_NORM_EXIT:
		strcpy(tmpstr, MCP3008_READ_NORM_EXIT_MSG);
		return;
	case MCP3008_READ_FAIL_NO_RX_NULL_BIT:
		strcpy(tmpstr, MCP3008_READ_FAIL_NO_RX_NULL_BIT_MSG);
		return;
	case MCP3008_READ_FAIL_OPEN_DEV:
		strcat(tmpstr, MCP3008_READ_FAIL_OPEN_DEV_MSG);
		strcat(tmpstr, device);
		strcat(tmpstr, " ");
		strcat(tmpstr, strerror(errno));
		return;
	case MCP3008_READ_FAIL_MISS_PARAM:
		strcpy(tmpstr, MCP3008_READ_FAIL_MISS_PARAM_MSG);
		return;
	case MCP3008_READ_FAIL_IOCTL:
		strcpy(tmpstr, MCP3008_READ_FAIL_IOCTL_MSG);
		return;
	case MCP3008_READ_FAIL_RX_ERR:
		strcpy(tmpstr, MCP3008_READ_FAIL_RX_ERR_MSG);
		return;
	case MCP3008_READ_FAIL_INTERNAL_ERR:
		strcpy(tmpstr, MCP3008_READ_FAIL_INTERNAL_ERR_MSG);
		return;
	default:
		strcpy(tmpstr, "");
		return;

	}
}

static          uint8_t
get_tx_word2(int ch)
{
	switch (ch) {
	case 0:
		return MCP3008_TX_WORD2_CH0;
	case 1:
		return MCP3008_TX_WORD2_CH1;
	case 2:
		return MCP3008_TX_WORD2_CH2;
	case 3:
		return MCP3008_TX_WORD2_CH3;
	case 4:
		return MCP3008_TX_WORD2_CH4;
	case 5:
		return MCP3008_TX_WORD2_CH5;
	case 6:
		return MCP3008_TX_WORD2_CH6;
	case 7:
		return MCP3008_TX_WORD2_CH7;
	default:
		fprintf(stderr, "Internal error\n");
		exit(EXIT_FAILURE);
	}

	return MCP3008_READ_NORM_EXIT;
}
