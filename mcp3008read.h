#ifndef _mcp3008read_h_
#define _mcp3008read_h_

/*
 * Needed includes in client and library application.
 */
#include <stdbool.h>		/* used in mcp3008read_attr_t for channel Flags 
				 */
#include <stdint.h>		/* used in mcp3008read_attr_t for spi_speed
				 * type uint32_t */
#include <linux/spi/spidev.h>	/* used in mcp3008read_attr_t for
				 * spi_ioc_transfer struct */
#include "uint10.h"		/* unsigned int 10 bit data type */

/*
 * MCP3008 related constants.
 */
#define MCP3008_CH_AMOUNT 8
#define MCP3008_BPW 8
#define MCP3008_MODE 0
#define MCP3008_LEN 3
#define MCP3008_DELAY 0
#define MCP3008_RESOLUTION 1023
#define MCP3008_SPEED_5V_MAX_HZ 3600000
#define MCP3008_SPEED_MIN_HZ 10000	/* 10kHz at 85 degree c. */

/*
 * Status codes used for return values of lib functions. 
 */
typedef enum e_mcp3008_ret_codes {
	MCP3008_READ_NORM_EXIT = 0,
	MCP3008_READ_FAIL_NO_RX_NULL_BIT = -1,
	MCP3008_READ_FAIL_OPEN_DEV = -2,
	MCP3008_READ_FAIL_MISS_PARAM = -3,
	MCP3008_READ_FAIL_IOCTL = -4,
	MCP3008_READ_FAIL_RX_ERR = -5,
	MCP3008_READ_FAIL_INTERNAL_ERR = -6,
} mcp3008read_state_t;

/*
 * 
 */
typedef struct {
	int             fd;
	uint32_t        spi_speed;
	bool            ch_flags[MCP3008_CH_AMOUNT];
	struct spi_ioc_transfer spi_ch_transfer;
} mcp3008read_attr_t;

/*
 * Channel nummbers.
 */
enum channels {
	CH0, CH1, CH2, CH3, CH4, CH5, CH6, CH7
};

/*
 * holds error codes 
 */
extern int      mcp3008errno;


/*
 * The mcp3008read_attr_init() function initializes the thread attributes 
 * object pointed to by attr.
 * @attr: Points to mcp3008read_attr_t representing the configuration of the 
 *        transfered data.
 * @spi_dev: String that represents the spi device in the filesystem.
 * @speed: Speed value that sets the SPI bus frequency in Hz.
 * @num_ch: Amount of channels that should be read followed by channel nummbers 
 *          defined in enum channels.
 * 
 * @return: On success zero is returned. On error, a negative value, defined 
 *          in mcp3008read_state_t, is returned, and mcp3008errno is set 
 *          appropriately.
 */
int             mcp3008read_attr_init(mcp3008read_attr_t * attr,
				      char *spi_dev, uint32_t speed,
				      int num_ch, ...);

/*
 * When a mcp3008read_attr_t attribute object is no longer required, it should
 * be destroyed using the mcp3008read_attr_destroy() function.
 * @attr: Points to mcp3008read_attr_t representing the configuration of the 
 *        transfered data that should be destroyed.
 *
 * @return: On success zero is returned. On error, a negative value, defined 
 *          in mcp3008read_state_t, is returned, and mcp3008errno is set 
 *          appropriately.
 */
int             mcp3008read_attr_destroy(mcp3008read_attr_t * attr);

/*
 * The mcp3008read() function executes the readout operation and stores the 
 * data in the data array.
 * @attr: Points to mcp3008read_attr_t representing the configuration of the 
 *        transfered data.
 * @data: Points to an array of type uint10_t with size 7 (*data[7]).
 *
 * @return: On success zero is returned. On error, a negative value, defined 
 *          in mcp3008read_state_t, is returned, and mcp3008errno is set 
 *          appropriately.
 */
int             mcp3008read(mcp3008read_attr_t * attr, uint10_t * data);

/*
 * The routine  pmcp3008error() produces a message on the standard error output,
 * describing the last error encountered during a call to a mcp3008read 
 * function. The error message is interpretet from the mcp3008errno value that 
 * holds an mcp3008read_state_t error code. If s is not NULL and *s is not a 
 * null byte ('\0')) the argument string s is printed, followed by a colon and 
 * a blank. Then the message and a new-line. 
 * @s: Points to a string that message should be printed additionally to the 
 *     mcp3008errno error message.
 */
void            pmcp3008error(char *s);

#endif
